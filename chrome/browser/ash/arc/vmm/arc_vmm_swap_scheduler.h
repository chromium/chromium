// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_VMM_ARC_VMM_SWAP_SCHEDULER_H_
#define CHROME_BROWSER_ASH_ARC_VMM_ARC_VMM_SWAP_SCHEDULER_H_

#include "base/functional/callback.h"
#include "base/timer/timer.h"

namespace arc {

// ArcVmmSwapScheduler periodically tries to swap out if it's suitable to enable
// VMM swap for ARCVM. It won't request to swap out within the given interval
// from the last swap out operation.
class ArcVmmSwapScheduler {
 public:
  ArcVmmSwapScheduler(base::TimeDelta minimum_swap_gap,
                      base::TimeDelta checking_period,
                      base::RepeatingCallback<bool()> swappable_checking_call,
                      base::RepeatingCallback<void(bool)> swap_call);
  ArcVmmSwapScheduler(const ArcVmmSwapScheduler&) = delete;
  ArcVmmSwapScheduler& operator=(const ArcVmmSwapScheduler&) = delete;
  ~ArcVmmSwapScheduler();

  void Start();

 private:
  void AttemptSwap();

  base::TimeDelta minimum_swap_gap_;
  base::TimeDelta checking_period_;

  base::RepeatingTimer timer_;

  // Callback returns true if the current ARCVM state is swappable.
  base::RepeatingCallback<bool()> swappable_checking_callback_;

  // Callback sends swap status to vmm manager.
  base::RepeatingCallback<void(bool)> swap_callback_;

  base::WeakPtrFactory<ArcVmmSwapScheduler> weak_ptr_factory_{this};
};
}  // namespace arc
#endif  // CHROME_BROWSER_ASH_ARC_VMM_ARC_VMM_SWAP_SCHEDULER_H_
