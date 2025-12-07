// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include <cstddef>

#include "partition_alloc/build_config.h"
#include "partition_alloc/buildflags.h"
#include "partition_alloc/oom.h"
#include "partition_alloc/partition_alloc_base/check.h"
#include "partition_alloc/partition_alloc_base/debug/alias.h"
#include "partition_alloc/partition_alloc_base/threading/platform_thread_for_testing.h"

#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
#include "partition_alloc/stack/stack.h"
#endif

namespace partition_alloc::internal::base {

namespace {

// base/win/scoped_handle.h looks too much to just run partition_alloc
// tests.
class ScopedHandle {
 public:
  ScopedHandle() : handle_(INVALID_HANDLE_VALUE) {}

  ~ScopedHandle() {
    if (handle_ != INVALID_HANDLE_VALUE) {
      CloseHandle(handle_);
    }
    handle_ = INVALID_HANDLE_VALUE;
  }

  void Set(HANDLE handle) {
    if (handle != handle_) {
      if (handle != INVALID_HANDLE_VALUE) {
        CloseHandle(handle_);
      }
      handle_ = handle;
    }
  }

 private:
  HANDLE handle_;
};

struct ThreadParams {
  PlatformThreadForTesting::Delegate* delegate = nullptr;
};

DWORD __stdcall ThreadFunc(void* params) {
  ThreadParams* thread_params = static_cast<ThreadParams*>(params);
  PlatformThreadForTesting::Delegate* delegate = thread_params->delegate;

  // Retrieve a copy of the thread handle to use as the key in the
  // thread name mapping.
  PlatformThreadHandle::Handle platform_handle;
  BOOL did_dup = DuplicateHandle(GetCurrentProcess(), GetCurrentThread(),
                                 GetCurrentProcess(), &platform_handle, 0,
                                 FALSE, DUPLICATE_SAME_ACCESS);

#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  StackTopRegistry::Get().NotifyThreadCreated();
#endif

  ScopedHandle scoped_platform_handle;
  if (did_dup) {
    scoped_platform_handle.Set(platform_handle);
  }

  delete thread_params;
  delegate->ThreadMain();

#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  StackTopRegistry::Get().NotifyThreadDestroyed();
#endif
  return 0;
}

// CreateThreadInternal() matches PlatformThread::CreateWithPriority(), except
// that |out_thread_handle| may be nullptr, in which case a non-joinable thread
// is created.
bool CreateThreadInternal(size_t stack_size,
                          PlatformThreadForTesting::Delegate* delegate,
                          PlatformThreadHandle* out_thread_handle) {
  unsigned int flags = 0;
  if (stack_size > 0) {
    flags = STACK_SIZE_PARAM_IS_A_RESERVATION;
#if PA_BUILDFLAG(PA_ARCH_CPU_32_BITS)
  } else {
    // The process stack size is increased to give spaces to |RendererMain| in
    // |chrome/BUILD.gn|, but keep the default stack size of other threads to
    // 1MB for the address space pressure.
    flags = STACK_SIZE_PARAM_IS_A_RESERVATION;
    static BOOL is_wow64 = -1;
    if (is_wow64 == -1 && !IsWow64Process(GetCurrentProcess(), &is_wow64)) {
      is_wow64 = FALSE;
    }
    // When is_wow64 is set that means we are running on 64-bit Windows and we
    // get 4 GiB of address space. In that situation we can afford to use 1 MiB
    // of address space for stacks. When running on 32-bit Windows we only get
    // 2 GiB of address space so we need to conserve. Typically stack usage on
    // these threads is only about 100 KiB.
    if (is_wow64) {
      stack_size = 1024 * 1024;
    } else {
      stack_size = 512 * 1024;
    }
#endif
  }

  ThreadParams* params = new ThreadParams;
  params->delegate = delegate;

  // Using CreateThread here vs _beginthreadex makes thread creation a bit
  // faster and doesn't require the loader lock to be available.  Our code will
  // have to work running on CreateThread() threads anyway, since we run code on
  // the Windows thread pool, etc.  For some background on the difference:
  // http://www.microsoft.com/msj/1099/win32/win321099.aspx
  void* thread_handle =
      ::CreateThread(nullptr, stack_size, ThreadFunc, params, flags, nullptr);

  if (!thread_handle) {
    DWORD last_error = ::GetLastError();

    switch (last_error) {
      case ERROR_NOT_ENOUGH_MEMORY:
      case ERROR_OUTOFMEMORY:
      case ERROR_COMMITMENT_LIMIT:
      case ERROR_COMMITMENT_MINIMUM:
        TerminateBecauseOutOfMemory(stack_size);

      default:
        break;
    }

    delete params;
    return false;
  }

  if (out_thread_handle) {
    *out_thread_handle = PlatformThreadHandle(thread_handle);
  } else {
    CloseHandle(thread_handle);
  }
  return true;
}

}  // namespace

// static
void PlatformThreadForTesting::YieldCurrentThread() {
  ::Sleep(0);
}

// static
void PlatformThreadForTesting::Join(PlatformThreadHandle thread_handle) {
  PA_BASE_DCHECK(thread_handle.platform_handle());

  DWORD thread_id = 0;
  thread_id = ::GetThreadId(thread_handle.platform_handle());
  DWORD last_error = 0;
  if (!thread_id) {
    last_error = ::GetLastError();
  }

  // Record information about the exiting thread in case joining hangs.
  base::debug::Alias(&thread_id);
  base::debug::Alias(&last_error);

  // Remove ScopedBlockingCallWithBaseSyncPrimitives, because only partition
  // alloc tests use PlatformThread::Join. So there is no special requirement
  // to monitor blocking calls
  // (by using ThreadGroupImpl::WorkerThreadDelegateImpl).
  //
  // base::internal::ScopedBlockingCallWithBaseSyncPrimitives
  //   scoped_blocking_call(base::BlockingType::MAY_BLOCK);

  // Wait for the thread to exit.  It should already have terminated but make
  // sure this assumption is valid.
  PA_BASE_CHECK(WAIT_OBJECT_0 ==
                WaitForSingleObject(thread_handle.platform_handle(), INFINITE));
  CloseHandle(thread_handle.platform_handle());
}

// static
bool PlatformThreadForTesting::Create(size_t stack_size,
                                      Delegate* delegate,
                                      PlatformThreadHandle* thread_handle) {
  PA_BASE_DCHECK(thread_handle);
  return CreateThreadInternal(stack_size, delegate, thread_handle);
}

}  // namespace partition_alloc::internal::base
