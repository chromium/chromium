// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_MODE_WATCHDOG_KIOSK_APP_LAUNCH_WATCHDOG_H_
#define CHROME_BROWSER_ASH_APP_MODE_WATCHDOG_KIOSK_APP_LAUNCH_WATCHDOG_H_

#include <string_view>

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

class PrefService;

namespace ash {

// Bounds the last phase of a Kiosk launch: the time between asking the
// `KioskAppLauncher` to launch the app (`KioskAppLauncher::LaunchApp`) and the
// app window being created (`KioskAppLauncher::Observer::OnAppWindowCreated`).
//
// When the timeout fires, the watchdog checks how many consecutive timeouts
// have occurred:
//   - If the timeout count is <= `kKioskAppLaunchWatchdogMaxOsReboots` (default
//     3), it logs `Action::kRebootDevice` to UMA and invokes `on_reboot`. The
//     OS reboots and the device starts up into the Kiosk auto-launch flow
//     again.
//   - Once the reboot budget is exhausted (>
//     `kKioskAppLaunchWatchdogMaxOsReboots`), it logs
//     `Action::kRebootsExhausted` to UMA without invoking `on_reboot`. Chrome
//     remains in the broken state with no further reboots.
//
// The number of consecutive timeouts is persisted in local state so that it
// survives the reboot. It is cleared as soon as an app window is successfully
// created.
class KioskAppLaunchWatchdog {
 public:
  // Recovery action chosen when the watchdog fires. Values are recorded to UMA,
  // do not renumber. Keep in sync with `KioskLaunchWatchdogAction` in
  // tools/metrics/histograms/enums.xml.
  enum class Action {
    kRebootDevice = 0,
    kRebootsExhausted = 1,
    kMaxValue = kRebootsExhausted,
  };

  // Key under `KioskChromeAppManager::kKioskDictionaryName` in local state
  // that stores the number of consecutive watchdog timeouts.
  static constexpr std::string_view kConsecutiveTimeoutsKey =
      "app_launch_watchdog_consecutive_timeouts";

  // Arms the watchdog on construction and disarms it on destruction (without
  // touching the persisted counter). `local_state` must outlive `this`.
  // `on_reboot` is invoked at most once when the timeout fires and the reboot
  // budget has not been exhausted. Must only be constructed when
  // `features::IsKioskAppLaunchWatchdogEnabled()` is true.
  KioskAppLaunchWatchdog(PrefService& local_state, base::OnceClosure on_reboot);
  KioskAppLaunchWatchdog(const KioskAppLaunchWatchdog&) = delete;
  KioskAppLaunchWatchdog& operator=(const KioskAppLaunchWatchdog&) = delete;
  ~KioskAppLaunchWatchdog();

  // Clears the persisted counter.
  void ClearRebootCounter();

 private:
  void OnTimeout();
  // Getter/setter for consecutive timeouts currently persisted in
  // `local_state`.
  int GetConsecutiveTimeouts() const;
  void SetConsecutiveTimeouts(int count);

  const raw_ref<PrefService> local_state_;
  base::OnceClosure on_reboot_;
  const base::TimeTicks armed_at_;
  int max_reboots_ = 0;
  base::OneShotTimer timer_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_APP_MODE_WATCHDOG_KIOSK_APP_LAUNCH_WATCHDOG_H_
