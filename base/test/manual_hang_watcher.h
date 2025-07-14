// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TEST_MANUAL_HANG_WATCHER_H_
#define BASE_TEST_MANUAL_HANG_WATCHER_H_

#include <atomic>

#include "base/functional/callback.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/hang_watcher.h"

namespace base::test {

// A version of `base::HangWatcher` that can be used in unit tests to check for
// hung threads manually and synchronously.
//
// In production, `HangWatcher` runs a periodic timer and checks whether threads
// are hung automatically on a background thread. This is unsuitable for unit
// tests, where we need monitoring to happen deterministically.
// `ManualHangWatcher` addresses this by disabling automated monitoring (by
// using a monitoring period of 365 days). Monitoring is instead manually
// triggered via `TriggerSynchronousMonitoring()`.
//
// Example usage:
//   ManualHangWatcher hang_watcher(HangWatcher::ProcessType::kBrowserProcess);
//   ScopedClosureRunner scoped_unregister = HangWatcher::RegisterThread(
//       HangWatcher::ThreadType::kMainThread);
//
//   HistogramTester histogram_tester;
//   hang_watcher.TriggerSynchronousMonitoring();  // Checks if thread is hung.
//   histogram_tester.ExpectBucketCount(
//       "HangWatcher.IsThreadHung.BrowserProcess.UIThread.Normal",
//       false, 1);  // Thread is not hung.
class ManualHangWatcher : public HangWatcher {
 public:
  explicit ManualHangWatcher(ProcessType process_type,
                             bool emit_crashes = true);

  ~ManualHangWatcher() override;

  // Set a callback to be invoked when a hang is detected.
  void SetOnHangClosure(RepeatingClosure closure);

  // Checks whether any of the watched threads are hung. The monitoring is done
  // in the hang watcher's monitoring thread. This function signals that thread
  // to start a monitoring pass and waits for it to complete.
  //
  // Results can be observed via histograms (using a `HistogramTester`), or by
  // watching for hangs using either `SetOnHangClosure()` or `GetHangCount()`.
  void TriggerSynchronousMonitoring();

  // Return the number of time a hang was observed.
  int GetHangCount() const;

 private:
  // Used to wait for monitoring. Will be signaled by the HangWatcher thread and
  // so needs to outlive it.
  WaitableEvent monitor_event_;

  // Count the number of time the HangWatcher thread detected a hang.
  std::atomic<int> hang_count_ = 0;

  // If specified by a test, this closure is invoked when a hang is detected.
  RepeatingClosure on_hang_closure_;
};

}  // namespace base::test

#endif  // BASE_TEST_MANUAL_HANG_WATCHER_H_
