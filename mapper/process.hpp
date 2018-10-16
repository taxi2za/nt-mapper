#pragma once
#include "memory_section.hpp"
#include "safe_handle.hpp"
#include "thread.hpp"

#include <windows.h>
#include <unordered_map>
#include <string>

namespace native
{
	class process
	{
	public:
		process() { }
		process(HANDLE handle) : m_handle(handle) {}
		process(std::uint32_t id, std::uint32_t desired_access) : m_handle(safe_handle(OpenProcess(desired_access, false, id))) { }
		process(std::string_view process_name, std::uint32_t desired_access)
		{
			const auto process_id = native::process::id_from_name(process_name);
			this->handle() = safe_handle(OpenProcess(desired_access, false, process_id));
		}

		explicit operator bool()
		{
			return this->handle().unsafe_handle() != nullptr;
		}

		// STATICS
		static process current_process();
		static std::uint32_t id_from_name(std::string_view process_name);

		// MEMORY
		std::uintptr_t map(memory_section& section);
		MEMORY_BASIC_INFORMATION virtual_query(const std::uintptr_t address);
		std::uintptr_t raw_allocate(const SIZE_T virtual_size, const std::uintptr_t address = 0);
		bool free_memory(const uintptr_t address);
		bool read_raw_memory(const void* buffer, const std::uintptr_t address, const std::size_t size);
		bool write_raw_memory(const void* buffer, const SIZE_T size, const std::uintptr_t address);
		bool virtual_protect(const std::uintptr_t address, std::uint32_t protect, std::uint32_t* old_protect);

		template <class T>
		__forceinline std::uintptr_t allocate_and_write(const T& buffer)
		{
			auto buffer_pointer = allocate(buffer);
			write_memory(buffer, buffer_pointer);
			return buffer_pointer;
		}

		template <class T>
		__forceinline std::uintptr_t allocate()
		{
			return raw_allocate(sizeof(T));
		}

		template<class T>
		__forceinline bool read_memory(T* buffer, const std::uintptr_t address)
		{
			return read_raw_memory(buffer, address, sizeof(T));
		}

		template<class T>
		__forceinline bool write_memory(const T& buffer, const std::uintptr_t address)
		{
			std::uint32_t old_protect;
			if (!this->virtual_protect(address, PAGE_EXECUTE_READWRITE, &old_protect))
			{
				//logger::log_error("Failed to set PAGE_EXECUTE_READWRITE");
				//logger::log_formatted("Last error", GetLastError(), true);
				//return false;
			}
			
			if (!write_raw_memory(reinterpret_cast<unsigned char*>(const_cast<T*>(&buffer)), sizeof(T), address))
			{
				//logger::log_error("Failed to write memory");
				//logger::log_formatted("Last error", GetLastError(), true);
				return false;
			}
			
			if (!this->virtual_protect(old_protect, PAGE_EXECUTE_READWRITE, &old_protect))
			{
				//logger::log_error("Failed to reset page protection");
				//logger::log_formatted("Last error", GetLastError(), true);
				//return false;
			}

			return true;
		}

		// INFORMATION
		HWND get_main_window();
		std::uint32_t get_id();
		std::unordered_map<std::string, std::uintptr_t> get_modules();
		std::string get_name();

		// PARSE EXPORTS
		struct module_export
		{
			// CTOR FROM FUNCTION POINTER
			module_export(std::uintptr_t new_function) :
				function(new_function), forwarded(false), forwarded_library(), forwarded_name() {}

			// CTOR FROM FORWARD INFO
			module_export(std::string_view library, std::string_view name) : 
				function(0x00), forwarded(true), forwarded_library(library), forwarded_name(name) {}

			std::uintptr_t function;
			bool forwarded;
			std::string forwarded_library;
			std::string forwarded_name;
		};
		native::process::module_export get_module_export(std::uintptr_t module_handle, const char* function_ordinal);

		// THREAD
		native::thread create_thread(const std::uintptr_t address, const std::uintptr_t argument = 0);

		std::vector<native::thread> threads();


		safe_handle& handle();

	private:
		safe_handle m_handle;
	};
}