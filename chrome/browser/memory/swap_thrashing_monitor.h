// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// SwapThrashingMonitor defines a state-based interface for a swap thrashing
// monitor. Thrashing is continuous swap activity, caused by processes needing
// to touch more pages than fit in physical memory, over a given period of time.
//
// The systems interested in observing these signals should query this monitor
// directly, it doesn't offer a notification API. Handling these signals should
// be done carefully in order to not aggravate the problem, in its initial
// implementation this monitor is meant to be used to measure the impact of
// thrashing on the core speed metrics.
//
// The different thrashing states are defined in the
// swap_thrashing_monitor_delegate.h header file.
//
// This monitor is responsible for initiating the observation of the
// swap-thrashing signals and it also enforce the state transition logic, the
// actual swap-thrashing observation logic should be done in a platform-specific
// implementation of the SwapThrashingDelegate class.

#ifndef CHROME_BROWSER_MEMORY_SWAP_THRASHING_MONITOR_H_
#define CHROME_BROWSER_MEMORY_SWAP_THRASHING_MONITOR_H_

#include <memory>

#include "base/base_export.h"
#include "base/feature_list.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/task/post_task.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "chrome/browser/memory/swap_thrashing_monitor_delegate.h"

namespace features {

extern const base::Feature kSwapThrashingMonitor;

}  // namespace features

namespace memory {

// Class for monitoring the swap-thrashing activity on the system.
//
// It is meant to be used as a singleton by calling the Initialize method,
// e.g.:
//     SwapThrashingMonitor::Initialize();
//
// Then the current thrashing level can be obtained by calling
// SwapThrashingMonitor::GetInstance()->GetCurrentSwapThrashingLevel();
//
// This class requires sequence-affinity, through use of ThreadChecker.
class SwapThrashingMonitor {
 public:
  // Sets the |SwapThrashingMonitor| global instance. This is a no-op if the
  // instance has already been initialized.
  static void Initialize();

  // Returns the |SwapThrashingMonitor| global instance.
  static SwapThrashingMonitor* GetInstance();

  // Returns the currently observed swap-thrashing pressure.
  SwapThrashingLevel GetCurrentSwapThrashingLevel();

 protected:
  SwapThrashingMonitor();
  virtual ~SwapThrashingMonitor();

  // Takes fresh measurements from the OS, calculates and stores the level from
  // them, and emits metrics if necessary.
  void CheckSwapThrashingPressureAndRecordStatistics();

  void StartObserving();

 private:
  void RecordSwapThrashingLevel(SwapThrashingLevel swap_thrashing_level);

  // The task runner used to run blocking operations.
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_ =
      base::CreateSequencedTaskRunner(
          {base::ThreadPool(), base::MayBlock(),
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN});

  // The delegate responsible for measuring the swap-thrashing activity. This
  // task runner is expected to be used and destroyed on
  // |blocking_task_runner_|.
  std::unique_ptr<SwapThrashingMonitorDelegate, base::OnTaskRunnerDeleter>
      delegate_;

  SwapThrashingLevel current_swap_thrashing_level_;

  base::RepeatingTimer check_timer_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<SwapThrashingMonitor> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SwapThrashingMonitor);
};

}  // namespace memory

#endif  // CHROME_BROWSER_MEMORY_SWAP_THRASHING_MONITOR_H_
