// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <pthread.h>
#include <sched.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <memory>

#include "partition_alloc/build_config.h"
#include "partition_alloc/buildflags.h"
#include "partition_alloc/partition_alloc_base/check.h"
#include "partition_alloc/partition_alloc_base/logging.h"
#include "partition_alloc/partition_alloc_base/threading/platform_thread_for_testing.h"
#include "partition_alloc/partition_alloc_base/threading/platform_thread_internal_posix.h"

#if PA_BUILDFLAG(IS_FUCHSIA)
#include <zircon/process.h>
#else
#include <sys/resource.h>
#endif

#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
#include "partition_alloc/stack/stack.h"
#endif

namespace partition_alloc::internal::base {

void InitThreading();
void TerminateOnThread();
size_t GetDefaultThreadStackSize(const pthread_attr_t& attributes);

namespace {

struct ThreadParams {
  PlatformThreadForTesting::Delegate* delegate = nullptr;
};

void* ThreadFunc(void* params) {
  PlatformThreadForTesting::Delegate* delegate = nullptr;

  {
    std::unique_ptr<ThreadParams> thread_params(
        static_cast<ThreadParams*>(params));

    delegate = thread_params->delegate;

#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
    StackTopRegistry::Get().NotifyThreadCreated();
#endif
  }

  delegate->ThreadMain();

#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  StackTopRegistry::Get().NotifyThreadDestroyed();
#endif

  TerminateOnThread();
  return nullptr;
}

bool CreateThread(size_t stack_size,
                  PlatformThreadForTesting::Delegate* delegate,
                  PlatformThreadHandle* thread_handle) {
  PA_BASE_DCHECK(thread_handle);
  base::InitThreading();

  pthread_attr_t attributes;
  pthread_attr_init(&attributes);

  // Get a better default if available.
  if (stack_size == 0) {
    stack_size = base::GetDefaultThreadStackSize(attributes);
  }

  if (stack_size > 0) {
    pthread_attr_setstacksize(&attributes, stack_size);
  }

  std::unique_ptr<ThreadParams> params(new ThreadParams);
  params->delegate = delegate;

  pthread_t handle;
  int err = pthread_create(&handle, &attributes, ThreadFunc, params.get());
  bool success = !err;
  if (success) {
    // ThreadParams should be deleted on the created thread after used.
    std::ignore = params.release();
  } else {
    // Value of |handle| is undefined if pthread_create fails.
    handle = 0;
    errno = err;
    PA_PLOG(ERROR) << "pthread_create";
  }
  *thread_handle = PlatformThreadHandle(handle);

  pthread_attr_destroy(&attributes);

  return success;
}

}  // namespace

#if !PA_BUILDFLAG(IS_APPLE)
// static
void PlatformThreadForTesting::YieldCurrentThread() {
  sched_yield();
}
#endif  // !PA_BUILDFLAG(IS_APPLE)

// static
bool PlatformThreadForTesting::Create(size_t stack_size,
                                      Delegate* delegate,
                                      PlatformThreadHandle* thread_handle) {
  return CreateThread(stack_size, delegate, thread_handle);
}

// static
void PlatformThreadForTesting::Join(PlatformThreadHandle thread_handle) {
  // Joining another thread may block the current thread for a long time, since
  // the thread referred to by |thread_handle| may still be running long-lived /
  // blocking tasks.

  // Remove ScopedBlockingCallWithBaseSyncPrimitives, because only partition
  // alloc tests use PlatformThread::Join. So there is no special requirement
  // to monitor blocking calls
  // (by using ThreadGroupImpl::WorkerThreadDelegateImpl).
  //
  // base::internal::ScopedBlockingCallWithBaseSyncPrimitives
  //   scoped_blocking_call(base::BlockingType::MAY_BLOCK);
  PA_BASE_CHECK(0 == pthread_join(thread_handle.platform_handle(), nullptr));
}

// static
size_t PlatformThreadForTesting::GetDefaultThreadStackSize() {
  pthread_attr_t attributes;
  pthread_attr_init(&attributes);
  return base::GetDefaultThreadStackSize(attributes);
}

}  // namespace partition_alloc::internal::base
