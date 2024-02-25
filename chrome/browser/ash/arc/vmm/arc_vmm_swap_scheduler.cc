// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/vmm/arc_vmm_swap_scheduler.h"

#include <memory>

#include "ash/components/arc/arc_prefs.h"
#include "ash/components/arc/arc_util.h"
#include "base/functional/bind.h"
#include "chrome/browser/ash/arc/vmm/arc_system_state_observation.h"
#include "chrome/browser/ash/arc/vmm/arc_vmm_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "components/prefs/pref_service.h"
#include "dbus/message.h"

namespace arc {

namespace {

PrefService* local_state() {
  return g_browser_process->local_state();
}
}  // namespace

ArcVmmSwapScheduler::ArcVmmSwapScheduler(
    base::RepeatingCallback<void(bool)> swap_callback,
    std::optional<base::TimeDelta> minimum_swapout_interval,
    std::optional<base::TimeDelta> swappable_checking_period,
    std::unique_ptr<PeaceDurationProvider> peace_duration_provider)
    : swap_callback_(swap_callback) {
  // Set callback to disable vmm-swap feature immdiately after the ARC get
  // activated.
  if (peace_duration_provider) {
    peace_duration_provider->SetDurationResetCallback(
        base::BindRepeating(&ArcVmmSwapScheduler::SetSwappable,
                            weak_ptr_factory_.GetWeakPtr(), false));
  }

  if (minimum_swapout_interval.has_value()) {
    SetSwapoutThrottleInterval(minimum_swapout_interval.value());
  }
  if (swappable_checking_period.has_value()) {
    SetActiveSwappableChecking(swappable_checking_period.value(),
                               std::move(peace_duration_provider));
  }
  auto* client = ash::ConciergeClient::Get();
  if (client) {
    vm_observer_.Observe(client);
  }
}

ArcVmmSwapScheduler::~ArcVmmSwapScheduler() = default;

void ArcVmmSwapScheduler::SetSwappable(bool swappable) {
  if (swappable) {
    swap_callback_.Run(true);
  } else {
    swap_callback_.Run(false);
  }
}

void ArcVmmSwapScheduler::OnVmSwapping(
    const vm_tools::concierge::VmSwappingSignal& signal) {
  if (signal.name() != kArcVmName) {
    return;
  }
  if (signal.state() == vm_tools::concierge::SWAPPING_OUT) {
    local_state()->SetTime(prefs::kArcVmmSwapOutTime, base::Time::Now());
  }
}

void ArcVmmSwapScheduler::SetSwapoutThrottleInterval(base::TimeDelta interval) {
  throttle_swapout_ = true;
  minimum_swapout_interval_ = interval;
}

void ArcVmmSwapScheduler::SetActiveSwappableChecking(
    base::TimeDelta period,
    std::unique_ptr<PeaceDurationProvider> peace_duration_provider) {
  DCHECK(peace_duration_provider);
  swappable_checking_period_ = period;
  peace_duration_provider_ = std::move(peace_duration_provider);
  swappable_checking_timer_.Start(
      FROM_HERE, period,
      base::BindRepeating(
          &ArcVmmSwapScheduler::UpdateSwappableStateByObservation,
          weak_ptr_factory_.GetWeakPtr()));
}

void ArcVmmSwapScheduler::UpdateSwappableStateByObservation() {
  if (throttle_swapout_) {
    const base::Time last_swap_out_time =
        local_state()->GetTime(prefs::kArcVmmSwapOutTime);

    if (!last_swap_out_time.is_null()) {
      auto past = base::Time::Now() - last_swap_out_time;
      if (past < minimum_swapout_interval_) {
        DVLOG(1) << "Swappable checking be throttled due to last swap on "
                 << last_swap_out_time
                 << " is not meet time interval requirement.";
        return;
      }
    }
  }

  // Add some randomize for "enable" state. This way can make the state
  // change in a uniform distribution [`swappable_checking_period_` * 0.5,
  // `swappable_checking_period_` * 1.5].
  SetSwappable(peace_duration_provider_->GetPeaceDuration() >
               swappable_checking_period_ / 2);
}

}  // namespace arc
