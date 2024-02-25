// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_VMM_ARC_VMM_SWAP_SCHEDULER_H_
#define CHROME_BROWSER_ASH_ARC_VMM_ARC_VMM_SWAP_SCHEDULER_H_

#include <optional>

#include "base/functional/callback.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/arc/vmm/arc_system_state_observation.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"

namespace arc {

class PeaceDurationProvider;

// ArcVmmSwapScheduler periodically tries to swap out if it's suitable to enable
// VMM swap for ARCVM. It won't request to swap out within the given interval
// from the last swap out operation.
class ArcVmmSwapScheduler : public ash::ConciergeClient::VmObserver {
 public:
  // ArcVmmSwapScheduler is allowed receive calls to set swap state and
  // control the swap state by itself.
  // If the `minimum_swapout_interval` is nullopt, the scheduler will not check
  // the time interval when "enable vmm swap".
  // If the `checking_period` is nullopt, the scheduler will not use the
  // `observation` and timer to check and control the swappable state.
  ArcVmmSwapScheduler(base::RepeatingCallback<void(bool)> swap_callback,
                      std::optional<base::TimeDelta> minimum_swapout_interval,
                      std::optional<base::TimeDelta> checking_period,
                      std::unique_ptr<PeaceDurationProvider> observation);

  ArcVmmSwapScheduler(const ArcVmmSwapScheduler&) = delete;
  ArcVmmSwapScheduler& operator=(const ArcVmmSwapScheduler&) = delete;
  ~ArcVmmSwapScheduler() override;

  void SetSwappable(bool swappable);

  // ash::ConciergeClient::VmObserver override:
  void OnVmSwapping(
      const vm_tools::concierge::VmSwappingSignal& signal) override;

 private:
  friend class ArcVmmManagerBrowserTest;
  void SetSwapoutThrottleInterval(base::TimeDelta interval);
  void SetActiveSwappableChecking(
      base::TimeDelta period,
      std::unique_ptr<PeaceDurationProvider> observation);

  void UpdateSwappableStateByObservation();

  // Swappable state throttle args.
  bool throttle_swapout_ = false;
  base::TimeDelta minimum_swapout_interval_;

  // Actively swappable checking args.
  base::TimeDelta swappable_checking_period_;
  base::RepeatingTimer swappable_checking_timer_;
  std::unique_ptr<PeaceDurationProvider> peace_duration_provider_;

  // Callback sends swap status to vmm manager.
  base::RepeatingCallback<void(bool)> swap_callback_;

  base::ScopedObservation<ash::ConciergeClient,
                          ash::ConciergeClient::VmObserver>
      vm_observer_{this};

  base::WeakPtrFactory<ArcVmmSwapScheduler> weak_ptr_factory_{this};
};
}  // namespace arc
#endif  // CHROME_BROWSER_ASH_ARC_VMM_ARC_VMM_SWAP_SCHEDULER_H_
