// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/vmm/arc_vmm_swap_scheduler.h"

#include "ash/components/arc/arc_prefs.h"
#include "chrome/browser/ash/arc/vmm/arc_vmm_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "components/prefs/pref_service.h"

namespace arc {

namespace {

PrefService* local_state() {
  return g_browser_process->local_state();
}
}  // namespace

ArcVmmSwapScheduler::ArcVmmSwapScheduler(
    base::TimeDelta minimum_swap_gap,
    base::TimeDelta checking_period,
    base::RepeatingCallback<bool()> swappable_checking_call,
    base::RepeatingCallback<void(bool)> swap_call)
    : minimum_swap_gap_(minimum_swap_gap),
      checking_period_(checking_period),
      swappable_checking_callback_(std::move(swappable_checking_call)),
      swap_callback_(std::move(swap_call)) {}

ArcVmmSwapScheduler::~ArcVmmSwapScheduler() = default;

void ArcVmmSwapScheduler::Start() {
  if (!timer_.IsRunning()) {
    timer_.Start(FROM_HERE, checking_period_,
                 base::BindRepeating(&ArcVmmSwapScheduler::AttemptSwap,
                                     weak_ptr_factory_.GetWeakPtr()));
  }
}

void ArcVmmSwapScheduler::AttemptSwap() {
  const base::Time last_swap_out_time =
      local_state()->GetTime(prefs::kArcVmmSwapOutTime);

  if (!last_swap_out_time.is_null()) {
    auto past = base::Time::Now() - last_swap_out_time;
    if (past < minimum_swap_gap_) {
      return;
    }
  }

  if (!swappable_checking_callback_.is_null() &&
      swappable_checking_callback_.Run()) {
    swap_callback_.Run(true);

    // TODO(sstan): Should be set by swap out notify.
    local_state()->SetTime(prefs::kArcVmmSwapOutTime, base::Time::Now());
  }
}

}  // namespace arc
