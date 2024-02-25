// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is a clone of "v8/src/base/platform/semaphore.h" in v8.
// Keep in sync, especially when fixing bugs.

// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_THREAD_POOL_SEMAPHORE_H_
#define BASE_TASK_THREAD_POOL_SEMAPHORE_H_

#include "base/base_export.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#elif BUILDFLAG(IS_MAC) || BUILDFLAG(IS_IOS)
#include <dispatch/dispatch.h>
#elif BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_NACL) || BUILDFLAG(IS_FUCHSIA)
#include <semaphore.h>
#else
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#endif

namespace base {
class TimeDelta;
namespace internal {

// ----------------------------------------------------------------------------
// Semaphore
//
// A semaphore object is a synchronization object that maintains a count. The
// count is decremented each time a thread completes a wait for the semaphore
// object and incremented each time a thread signals the semaphore. When the
// count reaches zero,  threads waiting for the semaphore blocks until the
// count becomes non-zero.

class BASE_EXPORT Semaphore {
 public:
  explicit Semaphore(int count);
  Semaphore(const Semaphore&) = delete;
  Semaphore& operator=(const Semaphore&) = delete;
  ~Semaphore();

  // Increments the semaphore counter.
  void Signal();

  // Decrements the semaphore counter if it is positive, or blocks until it
  // becomes positive and then decrements the counter.
  //
  // Wait's return "happens-after" |Signal| has completed. This means that it's
  // safe for a WaitableEvent to synchronise its own destruction, like this:
  //
  //   Semaphore* s = new Semaphore;
  //   SendToOtherThread(s);
  //   s->Wait();
  //   delete s;
  void Wait();

  // Like Wait() but returns after `timeout` time has passed. If the call times
  // out, the return value is false and the counter is unchanged. Otherwise the
  // semaphore counter is decremented and true is returned.
  //
  // Note: Timeout is checked to be no more than DWORD-size (24 days).
  [[nodiscard]] bool TimedWait(TimeDelta timeout);

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_IOS)
  using NativeHandle = dispatch_semaphore_t;
#elif BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_NACL) || BUILDFLAG(IS_FUCHSIA)
  using NativeHandle = sem_t;
#elif BUILDFLAG(IS_WIN)
  using NativeHandle = HANDLE;
#else  // default implementation
  using NativeHandle = struct DefaultSemaphore {
   private:
    friend class Semaphore;
    DefaultSemaphore(int count) : condition_var(&lock), value(count) {}

    Lock lock;
    ConditionVariable condition_var GUARDED_BY(lock);
    int value GUARDED_BY(lock);
  };
#endif

 private:
  NativeHandle& native_handle() { return native_handle_; }
  const NativeHandle& native_handle() const { return native_handle_; }

  NativeHandle native_handle_;
};

}  // namespace internal
}  // namespace base

#endif  // BASE_TASK_THREAD_POOL_SEMAPHORE_H_
