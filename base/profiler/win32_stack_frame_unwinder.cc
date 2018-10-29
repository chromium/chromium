// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/profiler/win32_stack_frame_unwinder.h"

#include <windows.h>

#include <utility>

#include "base/macros.h"

namespace base {

// Win32UnwindFunctions -------------------------------------------------------

const HMODULE ModuleHandleTraits::kNonNullModuleForTesting =
    reinterpret_cast<HMODULE>(static_cast<uintptr_t>(-1));

// static
bool ModuleHandleTraits::CloseHandle(HMODULE handle) {
  if (handle == kNonNullModuleForTesting)
    return true;

  return ::FreeLibrary(handle) != 0;
}

// static
bool ModuleHandleTraits::IsHandleValid(HMODULE handle) {
  return handle != nullptr;
}

// static
HMODULE ModuleHandleTraits::NullHandle() {
  return nullptr;
}

namespace {

// Implements the UnwindFunctions interface for the corresponding Win32
// functions.
class Win32UnwindFunctions : public Win32StackFrameUnwinder::UnwindFunctions {
public:
  Win32UnwindFunctions();
  ~Win32UnwindFunctions() override;

  PRUNTIME_FUNCTION LookupFunctionEntry(DWORD64 program_counter,
                                        PDWORD64 image_base) override;

  void VirtualUnwind(DWORD64 image_base,
                     DWORD64 program_counter,
                     PRUNTIME_FUNCTION runtime_function,
                     CONTEXT* context) override;

  ScopedModuleHandle GetModuleForProgramCounter(
      DWORD64 program_counter) override;

private:
  DISALLOW_COPY_AND_ASSIGN(Win32UnwindFunctions);
};

Win32UnwindFunctions::Win32UnwindFunctions() {}
Win32UnwindFunctions::~Win32UnwindFunctions() {}

PRUNTIME_FUNCTION Win32UnwindFunctions::LookupFunctionEntry(
    DWORD64 program_counter,
    PDWORD64 image_base) {
#ifdef _WIN64
  return ::RtlLookupFunctionEntry(program_counter, image_base, nullptr);
#else
  NOTREACHED();
  return nullptr;
#endif
}

void Win32UnwindFunctions::VirtualUnwind(DWORD64 image_base,
                                         DWORD64 program_counter,
                                         PRUNTIME_FUNCTION runtime_function,
                                         CONTEXT* context) {
#ifdef _WIN64
  void* handler_data;
  ULONG64 establisher_frame;
  KNONVOLATILE_CONTEXT_POINTERS nvcontext = {};
  ::RtlVirtualUnwind(UNW_FLAG_NHANDLER, image_base, program_counter,
                     runtime_function, context, &handler_data,
                     &establisher_frame, &nvcontext);
#else
  NOTREACHED();
#endif
}

ScopedModuleHandle Win32UnwindFunctions::GetModuleForProgramCounter(
    DWORD64 program_counter) {
  HMODULE module_handle = nullptr;
  // GetModuleHandleEx() increments the module reference count, which is then
  // managed and ultimately decremented by ScopedModuleHandle.
  if (!::GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                           reinterpret_cast<LPCTSTR>(program_counter),
                           &module_handle)) {
    const DWORD error = ::GetLastError();
    DCHECK_EQ(ERROR_MOD_NOT_FOUND, static_cast<int>(error));
  }
  return ScopedModuleHandle(module_handle);
}

}  // namespace

// Win32StackFrameUnwinder ----------------------------------------------------

Win32StackFrameUnwinder::UnwindFunctions::~UnwindFunctions() {}
Win32StackFrameUnwinder::UnwindFunctions::UnwindFunctions() {}

Win32StackFrameUnwinder::Win32StackFrameUnwinder()
    : Win32StackFrameUnwinder(std::make_unique<Win32UnwindFunctions>()) {}

Win32StackFrameUnwinder::~Win32StackFrameUnwinder() {}

bool Win32StackFrameUnwinder::TryUnwind(CONTEXT* context,
                                        ScopedModuleHandle* module) {
#ifdef _WIN64
  // TODO(chengx): update base::ModuleCache to return a ScopedModuleHandle and
  // use it for this module lookup.
  ScopedModuleHandle frame_module =
      unwind_functions_->GetModuleForProgramCounter(context->Rip);
  if (!frame_module.IsValid()) {
    // There's no loaded module containing the instruction pointer. This can be
    // due to executing code that is not in a module. In particular,
    // runtime-generated code associated with third-party injected DLLs
    // typically is not in a module. It can also be due to the the module having
    // been unloaded since we recorded the stack.  In the latter case the
    // function unwind information was part of the unloaded module, so it's not
    // possible to unwind further.
    //
    // If a module was found, it's still theoretically possible for the detected
    // module module to be different than the one that was loaded when the stack
    // was copied (i.e. if the module was unloaded and a different module loaded
    // in overlapping memory). This likely would cause a crash, but has not been
    // observed in practice.
    return false;
  }

  ULONG64 image_base;
  // Try to look up unwind metadata for the current function.
  PRUNTIME_FUNCTION runtime_function =
      unwind_functions_->LookupFunctionEntry(context->Rip, &image_base);

  if (runtime_function) {
    unwind_functions_->VirtualUnwind(image_base, context->Rip, runtime_function,
                                     context);
    at_top_frame_ = false;
  } else {
    if (at_top_frame_) {
      at_top_frame_ = false;

      // This is a leaf function (i.e. a function that neither calls a function,
      // nor allocates any stack space itself) so the return address is at RSP.
      context->Rip = *reinterpret_cast<DWORD64*>(context->Rsp);
      context->Rsp += 8;
    } else {
      // In theory we shouldn't get here, as it means we've encountered a
      // function without unwind information below the top of the stack, which
      // is forbidden by the Microsoft x64 calling convention.
      //
      // The one known case in Chrome code that executes this path occurs
      // because of BoringSSL unwind information inconsistent with the actual
      // function code. See https://crbug.com/542919.
      //
      // Note that dodgy third-party generated code that otherwise would enter
      // this path should be caught by the module check above, since the code
      // typically is located outside of a module.
      return false;
    }
  }

  module->Set(frame_module.Take());
  return true;
#else
  NOTREACHED();
  return false;
#endif
}

Win32StackFrameUnwinder::Win32StackFrameUnwinder(
    std::unique_ptr<UnwindFunctions> unwind_functions)
    : at_top_frame_(true), unwind_functions_(std::move(unwind_functions)) {}

}  // namespace base
