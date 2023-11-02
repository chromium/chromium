// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/handle_hooks_win.h"

#include <windows.h>

#include <psapi.h>
#include <stddef.h>

#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/numerics/safe_conversions.h"
#include "base/win/iat_patch_function.h"
#include "base/win/pe_image.h"
#include "base/win/scoped_handle.h"
#include "build/build_config.h"

namespace {

using CloseHandleType = decltype(&::CloseHandle);
using DuplicateHandleType = decltype(&::DuplicateHandle);

CloseHandleType g_close_function = nullptr;
DuplicateHandleType g_duplicate_function = nullptr;

// The entry point for CloseHandle interception. This function notifies the
// verifier about the handle that is being closed, and calls the original
// function.
BOOL WINAPI CloseHandleHook(HANDLE handle) {
  base::win::OnHandleBeingClosed(handle,
                                 base::win::HandleOperation::kCloseHandleHook);
  return g_close_function(handle);
}

BOOL WINAPI DuplicateHandleHook(HANDLE source_process,
                                HANDLE source_handle,
                                HANDLE target_process,
                                HANDLE* target_handle,
                                DWORD desired_access,
                                BOOL inherit_handle,
                                DWORD options) {
  if ((options & DUPLICATE_CLOSE_SOURCE) &&
      (GetProcessId(source_process) == ::GetCurrentProcessId())) {
    base::win::OnHandleBeingClosed(
        source_handle, base::win::HandleOperation::kDuplicateHandleHook);
  }

  return g_duplicate_function(source_process, source_handle, target_process,
                              target_handle, desired_access, inherit_handle,
                              options);
}

}  // namespace

namespace base {
namespace debug {

namespace {

// Provides a simple way to temporarily change the protection of a memory page.
class AutoProtectMemory {
 public:
  AutoProtectMemory()
      : changed_(false), address_(nullptr), bytes_(0), old_protect_(0) {}

  AutoProtectMemory(const AutoProtectMemory&) = delete;
  AutoProtectMemory& operator=(const AutoProtectMemory&) = delete;

  ~AutoProtectMemory() { RevertProtection(); }

  // Grants write access to a given memory range.
  bool ChangeProtection(void* address, size_t bytes);

  // Restores the original page protection.
  void RevertProtection();

 private:
  bool changed_;
  raw_ptr<void> address_;
  size_t bytes_;
  DWORD old_protect_;
};

bool AutoProtectMemory::ChangeProtection(void* address, size_t bytes) {
  DCHECK(!changed_);
  DCHECK(address);

  // Change the page protection so that we can write.
  MEMORY_BASIC_INFORMATION memory_info;
  if (!VirtualQuery(address, &memory_info, sizeof(memory_info)))
    return false;

  DWORD is_executable = (PAGE_EXECUTE | PAGE_EXECUTE_READ |
                         PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY) &
                        memory_info.Protect;

  DWORD protect = is_executable ? PAGE_EXECUTE_READWRITE : PAGE_READWRITE;
  if (!VirtualProtect(address, bytes, protect, &old_protect_))
    return false;

  changed_ = true;
  address_ = address;
  bytes_ = bytes;
  return true;
}

void AutoProtectMemory::RevertProtection() {
  if (!changed_)
    return;

  DCHECK(address_);
  DCHECK(bytes_);

  VirtualProtect(address_, bytes_, old_protect_, &old_protect_);
  changed_ = false;
  address_ = nullptr;
  bytes_ = 0;
  old_protect_ = 0;
}

#if defined(ARCH_CPU_32_BITS)
// Performs an EAT interception. Only supported on 32-bit.
void EATPatch(HMODULE module,
              const char* function_name,
              void* new_function,
              void** old_function) {
  if (!module)
    return;

  base::win::PEImage pe(module);
  if (!pe.VerifyMagic())
    return;

  DWORD* eat_entry = pe.GetExportEntry(function_name);
  if (!eat_entry)
    return;

  if (!(*old_function))
    *old_function = pe.RVAToAddr(*eat_entry);

  AutoProtectMemory memory;
  if (!memory.ChangeProtection(eat_entry, sizeof(DWORD)))
    return;

  // Perform the patch.
  *eat_entry =
      base::checked_cast<DWORD>(reinterpret_cast<uintptr_t>(new_function) -
                                reinterpret_cast<uintptr_t>(module));
}
#endif  // defined(ARCH_CPU_32_BITS)

// Performs an IAT interception.
std::unique_ptr<base::win::IATPatchFunction> IATPatch(HMODULE module,
                                                      const char* function_name,
                                                      void* new_function,
                                                      void** old_function) {
  if (!module)
    return nullptr;

  auto patch = std::make_unique<base::win::IATPatchFunction>();
  __try {
    // There is no guarantee that |module| is still loaded at this point.
    if (patch->PatchFromModule(module, "kernel32.dll", function_name,
                               new_function)) {
      return nullptr;
    }
  } __except ((GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ||
               GetExceptionCode() == EXCEPTION_GUARD_PAGE ||
               GetExceptionCode() == EXCEPTION_IN_PAGE_ERROR)
                  ? EXCEPTION_EXECUTE_HANDLER
                  : EXCEPTION_CONTINUE_SEARCH) {
    // Leak the patch.
    std::ignore = patch.release();
    return nullptr;
  }

  if (!(*old_function)) {
    // Things are probably messed up if each intercepted function points to
    // a different place, but we need only one function to call.
    *old_function = patch->original_function();
  }
  return patch;
}

}  // namespace

// static
void HandleHooks::AddIATPatch(HMODULE module) {
  if (!module)
    return;

  auto close_handle_patch =
      IATPatch(module, "CloseHandle", reinterpret_cast<void*>(&CloseHandleHook),
               reinterpret_cast<void**>(&g_close_function));
  if (!close_handle_patch)
    return;
  // This is intentionally leaked.
  std::ignore = close_handle_patch.release();

  auto duplicate_handle_patch = IATPatch(
      module, "DuplicateHandle", reinterpret_cast<void*>(&DuplicateHandleHook),
      reinterpret_cast<void**>(&g_duplicate_function));
  if (!duplicate_handle_patch)
    return;
  // This is intentionally leaked.
  std::ignore = duplicate_handle_patch.release();
}

#if defined(ARCH_CPU_32_BITS)
// static
void HandleHooks::AddEATPatch() {
  // An attempt to restore the entry on the table at destruction is not safe.
  EATPatch(GetModuleHandleA("kernel32.dll"), "CloseHandle",
           reinterpret_cast<void*>(&CloseHandleHook),
           reinterpret_cast<void**>(&g_close_function));
  EATPatch(GetModuleHandleA("kernel32.dll"), "DuplicateHandle",
           reinterpret_cast<void*>(&DuplicateHandleHook),
           reinterpret_cast<void**>(&g_duplicate_function));
}
#endif  // defined(ARCH_CPU_32_BITS)

// static
void HandleHooks::PatchLoadedModules() {
  const DWORD kSize = 256;
  DWORD returned;
  auto modules = std::make_unique<HMODULE[]>(kSize);
  if (!::EnumProcessModules(GetCurrentProcess(), modules.get(),
                            kSize * sizeof(HMODULE), &returned)) {
    return;
  }
  returned /= sizeof(HMODULE);
  returned = std::min(kSize, returned);

  for (DWORD current = 0; current < returned; current++) {
    AddIATPatch(modules[current]);
  }
}

}  // namespace debug
}  // namespace base
