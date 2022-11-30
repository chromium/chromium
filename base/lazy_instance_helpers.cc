// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/lazy_instance_helpers.h"

#include <atomic>

#include "base/at_exit.h"
#include "base/threading/platform_thread.h"

#include <dlfcn.h>

namespace base {
namespace internal {

<<<<<<< HEAD
static void (*gRecordReplayBeginPassThroughEventsFn)();

static void RecordReplayBeginPassThroughEvents() {
  if (!gRecordReplayBeginPassThroughEventsFn) {
    void* fnptr = dlsym(RTLD_DEFAULT, "RecordReplayBeginPassThroughEvents");
    if (!fnptr) {
      return;
    }
    gRecordReplayBeginPassThroughEventsFn = reinterpret_cast<void(*)()>(fnptr);
  }

  gRecordReplayBeginPassThroughEventsFn();
}

static void (*gRecordReplayEndPassThroughEventsFn)();

static void RecordReplayEndPassThroughEvents() {
  if (!gRecordReplayEndPassThroughEventsFn) {
    void* fnptr = dlsym(RTLD_DEFAULT, "RecordReplayEndPassThroughEvents");
    if (!fnptr) {
      return;
    }
    gRecordReplayEndPassThroughEventsFn = reinterpret_cast<void(*)()>(fnptr);
  }

  gRecordReplayEndPassThroughEventsFn();
}

bool NeedsLazyInstance(subtle::AtomicWord* state) {
||||||| 80c960997e61f
bool NeedsLazyInstance(subtle::AtomicWord* state) {
=======
bool NeedsLazyInstance(std::atomic<uintptr_t>& state) {
>>>>>>> 27d3765d341b09369006d030f83f582a29eb57ae
  // Try to create the instance, if we're the first, will go from 0 to
  // kLazyInstanceStateCreating, otherwise we've already been beaten here.
  // The memory access has no memory ordering as state 0 and
  // kLazyInstanceStateCreating have no associated data (memory barriers are
  // all about ordering of memory accesses to *associated* data).
  uintptr_t expected = 0;
  if (state.compare_exchange_strong(expected, kLazyInstanceStateCreating,
                                    std::memory_order_relaxed,
                                    std::memory_order_relaxed)) {
    // Caller must create instance
    return true;
  }

  // It's either in the process of being created, or already created. Spin.
  // The load has acquire memory ordering as a thread which sees
  // state_ == STATE_CREATED needs to acquire visibility over
  // the associated data (buf_). Pairing Release_Store is in
  // CompleteLazyInstance().
<<<<<<< HEAD
  if (subtle::Acquire_Load(state) == kLazyInstanceStateCreating) {
    // Don't interact with the recording while we get the current time or sleep
    // in non-deterministic ways.
    RecordReplayBeginPassThroughEvents();

||||||| 80c960997e61f
  if (subtle::Acquire_Load(state) == kLazyInstanceStateCreating) {
=======
  if (state.load(std::memory_order_acquire) == kLazyInstanceStateCreating) {
>>>>>>> 27d3765d341b09369006d030f83f582a29eb57ae
    const base::TimeTicks start = base::TimeTicks::Now();
    do {
      const base::TimeDelta elapsed = base::TimeTicks::Now() - start;
      // Spin with YieldCurrentThread for at most one ms - this ensures
      // maximum responsiveness. After that spin with Sleep(1ms) so that we
      // don't burn excessive CPU time - this also avoids infinite loops due
      // to priority inversions (https://crbug.com/797129).
      if (elapsed < Milliseconds(1))
        PlatformThread::YieldCurrentThread();
      else
<<<<<<< HEAD
        PlatformThread::Sleep(TimeDelta::FromMilliseconds(1));
    } while (subtle::Acquire_Load(state) == kLazyInstanceStateCreating);

    RecordReplayEndPassThroughEvents();
||||||| 80c960997e61f
        PlatformThread::Sleep(TimeDelta::FromMilliseconds(1));
    } while (subtle::Acquire_Load(state) == kLazyInstanceStateCreating);
=======
        PlatformThread::Sleep(Milliseconds(1));
    } while (state.load(std::memory_order_acquire) ==
             kLazyInstanceStateCreating);
>>>>>>> 27d3765d341b09369006d030f83f582a29eb57ae
  }
  // Someone else created the instance.
  return false;
}

void CompleteLazyInstance(std::atomic<uintptr_t>& state,
                          uintptr_t new_instance,
                          void (*destructor)(void*),
                          void* destructor_arg) {
  // Instance is created, go from CREATING to CREATED (or reset it if
  // |new_instance| is null). Releases visibility over |private_buf_| to
  // readers. Pairing Acquire_Load is in NeedsLazyInstance().
  state.store(new_instance, std::memory_order_release);

  // Make sure that the lazily instantiated object will get destroyed at exit.
  if (new_instance && destructor)
    AtExitManager::RegisterCallback(destructor, destructor_arg);
}

}  // namespace internal
}  // namespace base
