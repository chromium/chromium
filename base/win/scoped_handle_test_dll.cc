// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include <vector>

#include "base/win/base_win_buildflags.h"
#include "base/win/current_module.h"
#include "base/win/scoped_handle.h"
#include "base/win/scoped_handle_verifier.h"

namespace base {
namespace win {
namespace testing {

extern "C" bool __declspec(dllexport) RunTest();

namespace {

struct ThreadParams {
  HANDLE ready_event;
  HANDLE start_event;
};

// Note, this must use all native functions to avoid instantiating the
// HandleVerifier. e.g. can't use base::Thread or even base::PlatformThread.
DWORD __stdcall ThreadFunc(void* params) {
  ThreadParams* thread_params = reinterpret_cast<ThreadParams*>(params);
  HANDLE handle = ::CreateMutex(nullptr, false, nullptr);

  ::SetEvent(thread_params->ready_event);
  ::WaitForSingleObject(thread_params->start_event, INFINITE);
  CheckedScopedHandle handle_holder(handle);
  return 0;
}

bool InternalRunThreadTest() {
  std::vector<HANDLE> threads_;
  // From manual testing, the bug fixed by crrev.com/678736a starts reliably
  // causing handle verifier asserts to trigger at around 100 threads, so make
  // it 200 to be sure to detect any future regressions.
  const size_t kNumThreads = 200;

  // bManualReset is set to true to allow signalling multiple threads.
  HANDLE start_event = ::CreateEvent(nullptr, true, false, nullptr);
  if (!start_event)
    return false;

  HANDLE ready_event = CreateEvent(nullptr, false, false, nullptr);
  if (!ready_event)
    return false;

  ThreadParams thread_params = {ready_event, start_event};

  for (size_t i = 0; i < kNumThreads; i++) {
    HANDLE thread_handle =
        ::CreateThread(nullptr, 0, ThreadFunc,
                       reinterpret_cast<void*>(&thread_params), 0, nullptr);
    if (!thread_handle)
      break;
    ::WaitForSingleObject(ready_event, INFINITE);
    threads_.push_back(thread_handle);
  }

  ::CloseHandle(ready_event);

  if (threads_.size() != kNumThreads) {
    for (auto* thread : threads_)
      ::CloseHandle(thread);
    ::CloseHandle(start_event);
    return false;
  }

  ::SetEvent(start_event);
  ::CloseHandle(start_event);
  for (auto* thread : threads_) {
    ::WaitForSingleObject(thread, INFINITE);
    ::CloseHandle(thread);
  }

  return true;
}

bool InternalRunLocationTest() {
  // Create a new handle and then set LastError again.
  HANDLE handle = ::CreateMutex(nullptr, false, nullptr);
  if (!handle)
    return false;
  CheckedScopedHandle handle_holder(handle);

  HMODULE verifier_module =
      base::win::internal::GetHandleVerifierModuleForTesting();
  if (!verifier_module)
    return false;

  // Get my module
  HMODULE my_module = CURRENT_MODULE();
  if (!my_module)
    return false;

  HMODULE main_module = ::GetModuleHandle(nullptr);

#if BUILDFLAG(SINGLE_MODULE_MODE_HANDLE_VERIFIER)
  // In a component build HandleVerifier will always be created inside base.dll
  // as the code always lives there.
  if (verifier_module == my_module || verifier_module == main_module)
    return false;
#else
  // In a non-component build, HandleVerifier should always be created in the
  // version of base linked with the main executable.
  if (verifier_module == my_module || verifier_module != main_module)
    return false;
#endif
  return true;
}

}  // namespace

bool RunTest() {
  return InternalRunThreadTest() && InternalRunLocationTest();
}

}  // namespace testing
}  // namespace win
}  // namespace base
