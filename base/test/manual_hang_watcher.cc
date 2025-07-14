// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/manual_hang_watcher.h"

#include <atomic>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/bind.h"
#include "base/time/time.h"

namespace base::test {

ManualHangWatcher::ManualHangWatcher(ProcessType process_type,
                                     bool emit_crashes) {
  HangWatcher::InitializeOnMainThread(process_type, emit_crashes);

  SetAfterMonitorClosureForTesting(
      BindRepeating(&WaitableEvent::Signal, Unretained(&monitor_event_)));

  SetOnHangClosureForTesting(BindLambdaForTesting([this] {
    hang_count_.fetch_add(1, std::memory_order_relaxed);
    if (on_hang_closure_) {
      on_hang_closure_.Run();
    }
  }));

  // Disable periodic monitoring by setting a very very long monitoring
  // period. Monitoring will be started manually by calling
  // `TriggerSynchronousMonitoring()`.
  SetMonitoringPeriodForTesting(Days(365));

  // Start the monitoring loop.
  Start();
}

ManualHangWatcher::~ManualHangWatcher() {
  UninitializeOnMainThreadForTesting();

  // Stop now instead of in `~HangWatcher()` to avoid a data race between
  // the destructor and virtual calls. If we destroy `HangWatcher` right after
  // it's created, `HangWatcher::Run()` might get called concurrently with
  // `~HangWatcher`. The vtable pointer is changed when calling into a parent
  // class destructor. Virtual calls might resolve differently before or after
  // the vtable is changed. See here for details:
  // https://github.com/google/sanitizers/wiki/ThreadSanitizerPopularDataRaces#data-race-on-vptr
  Stop();
}

void ManualHangWatcher::SetOnHangClosure(RepeatingClosure closure) {
  on_hang_closure_ = std::move(closure);
}

void ManualHangWatcher::TriggerSynchronousMonitoring() {
  monitor_event_.Reset();
  SignalMonitorEventForTesting();
  monitor_event_.Wait();
}

int ManualHangWatcher::GetHangCount() const {
  return hang_count_.load(std::memory_order_relaxed);
}

}  // namespace base::test
