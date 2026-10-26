// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/watchdog/kiosk_app_launch_watchdog.h"

#include <string_view>
#include <utility>

#include "ash/constants/ash_features.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/syslog_logging.h"
#include "base/time/time.h"
#include "chrome/browser/ash/app_mode/kiosk_chrome_app_manager.h"
#include "chrome/browser/ash/app_mode/kiosk_launch_state.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace ash {

namespace {

constexpr char kActionHistogram[] = "Kiosk.Launch.WatchdogAction";

}  // namespace

KioskAppLaunchWatchdog::KioskAppLaunchWatchdog(PrefService& local_state,
                                               base::OnceClosure on_reboot)
    : local_state_(local_state),
      on_reboot_(std::move(on_reboot)),
      armed_at_(base::TimeTicks::Now()) {
  CHECK(features::IsKioskAppLaunchWatchdogEnabled());
  CHECK(on_reboot_);

  max_reboots_ = features::kKioskAppLaunchWatchdogMaxOsReboots.Get();
  const base::TimeDelta timeout =
      features::kKioskAppLaunchWatchdogTimeout.Get();
  SYSLOG(INFO) << "Kiosk app launch watchdog armed, timeout=" << timeout
               << ", consecutive timeouts so far=" << GetConsecutiveTimeouts();

  // `base::Unretained` is safe because `timer_` is owned by `this` and is
  // stopped in the destructor.
  timer_.Start(FROM_HERE, timeout,
               base::BindOnce(&KioskAppLaunchWatchdog::OnTimeout,
                              base::Unretained(this)));
}

KioskAppLaunchWatchdog::~KioskAppLaunchWatchdog() = default;

void KioskAppLaunchWatchdog::ClearRebootCounter() {
  const int timeouts = GetConsecutiveTimeouts();
  if (timeouts > 0) {
    SYSLOG(INFO) << "Kiosk app window created after " << timeouts
                 << " watchdog timeout(s), resetting counter.";
    SetConsecutiveTimeouts(0);
  }
}

int KioskAppLaunchWatchdog::GetConsecutiveTimeouts() const {
  return local_state_->GetDict(KioskChromeAppManager::kKioskDictionaryName)
      .FindInt(kConsecutiveTimeoutsKey)
      .value_or(0);
}

void KioskAppLaunchWatchdog::SetConsecutiveTimeouts(int count) {
  {
    ScopedDictPrefUpdate dict_update(
        &local_state_.get(), KioskChromeAppManager::kKioskDictionaryName);
    if (count > 0) {
      dict_update->Set(kConsecutiveTimeoutsKey, count);
    } else {
      dict_update->Remove(kConsecutiveTimeoutsKey);
    }
  }
  // Make sure the counter reaches the disk before the device is rebooted.
  local_state_->CommitPendingWrite();
}

void KioskAppLaunchWatchdog::OnTimeout() {
  const int timeouts = GetConsecutiveTimeouts() + 1;
  SetConsecutiveTimeouts(timeouts);

  const bool can_reboot = timeouts <= max_reboots_;
  const Action action =
      can_reboot ? Action::kRebootDevice : Action::kRebootsExhausted;

  if (can_reboot) {
    SYSLOG(ERROR) << "Kiosk app window was not created within "
                  << base::TimeTicks::Now() - armed_at_
                  << " after launch was requested (consecutive timeout "
                  << timeouts << "/" << max_reboots_
                  << "). Assuming the app is hung, rebooting device.";
  } else {
    SYSLOG(ERROR) << "Kiosk app window was not created within "
                  << base::TimeTicks::Now() - armed_at_
                  << " after launch was requested (consecutive timeouts: "
                  << timeouts << ", reboot budget " << max_reboots_
                  << " exhausted). Assuming the app is hung; taking no further "
                  << "action and remaining in broken state.";
  }

  base::UmaHistogramEnumeration(kActionHistogram, action);
  SetKioskLaunchStateCrashKey(KioskLaunchState::kAppLaunchTimedOut);

  if (can_reboot) {
    std::move(on_reboot_).Run();
  }
}

}  // namespace ash
