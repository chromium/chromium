// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POWER_AUTO_SCREEN_BRIGHTNESS_BRIGHTNESS_MONITOR_IMPL_H_
#define CHROME_BROWSER_ASH_POWER_AUTO_SCREEN_BRIGHTNESS_BRIGHTNESS_MONITOR_IMPL_H_

#include <optional>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/power/auto_screen_brightness/brightness_monitor.h"
#include "chromeos/dbus/power/power_manager_client.h"

namespace ash {
namespace power {
namespace auto_screen_brightness {

// Real implementation of BrightnessMonitor.
// It monitors user brightness changes and records the stabilized brightness.
class BrightnessMonitorImpl : public BrightnessMonitor,
                              public chromeos::PowerManagerClient::Observer {
 public:
  // Once a user brightness adjustment is received, we wait for
  // |brightness_sample_delay_| to record the final brightness. It can be
  // configured from finch with default value set to |kBrightnessSampleDelay|.
  static constexpr base::TimeDelta kBrightnessSampleDelay = base::Seconds(3);

  BrightnessMonitorImpl();

  BrightnessMonitorImpl(const BrightnessMonitorImpl&) = delete;
  BrightnessMonitorImpl& operator=(const BrightnessMonitorImpl&) = delete;

  ~BrightnessMonitorImpl() override;

  // Must be called before the BrightnessMonitorImpl is used.
  void Init();

  // BrightnessMonitor overrides:
  void AddObserver(BrightnessMonitor::Observer* observer) override;
  void RemoveObserver(BrightnessMonitor::Observer* observer) override;

  // chromeos::PowerManagerClient::Observer overrides:
  void PowerManagerBecameAvailable(bool service_is_ready) override;
  void ScreenBrightnessChanged(
      const power_manager::BacklightBrightnessChange& change) override;

  base::TimeDelta GetBrightnessSampleDelayForTesting() const;

 private:
  // Sets initial brightness obtained from powerd. If nullopt is received from
  // powerd, the monitor status will be set to kDisabled.
  void OnReceiveInitialBrightnessPercent(
      std::optional<double> brightness_percent);

  // Notifies its observers on the initialization status of the monitor.
  void OnInitializationComplete();

  // Called when a user-triggered brightness change signal is received. We start
  // the |brightness_sample_timer_| to wait for brightness to stabilize and
  // collect the final brightness.
  void StartBrightnessSampleTimer();

  // Called when |brightness_sample_timer_| times out or a non-user-initiated
  // change is received while the timer is running. We take the final brightness
  // stored in |user_brightness_percent_| as the final user selected brightness.
  void NotifyUserBrightnessChanged();

  // Called as soon as a user-triggered brightness event is received.
  void NotifyUserBrightnessChangeRequested();

  base::ScopedObservation<chromeos::PowerManagerClient,
                          chromeos::PowerManagerClient::Observer>
      power_manager_client_observation_{this};

  // Delay after user brightness adjustment before we record the brightness.
  base::TimeDelta brightness_sample_delay_;

  // This timer is started when we receive the 1st user-requested brightness
  // change and times out after kBrightnessSampleDelay if there are no more
  // user-requested changes. The timer is reset if there is another
  // user-requested change before it times out. The timer stops immediately if a
  // non-user-requested change is received.
  base::OneShotTimer brightness_sample_timer_;

  BrightnessMonitor::Status brightness_monitor_status_ =
      BrightnessMonitor::Status::kInitializing;

  // Current brightness. It is updated when brightness change is reported by
  // powerd. If the change is user requested, it will store the
  // final/consolidated brightness (i.e. ignoring intermediate values selected
  // by the user). If the change is not user requested, it will simply be the
  // new brightness value.
  std::optional<double> stable_brightness_percent_;
  // Current user selected brightness. It is reset after we've collected
  // final/stable user-requested brightness (i.e. after
  // |brightness_sample_timer_| times out).
  std::optional<double> user_brightness_percent_;

  base::ObserverList<BrightnessMonitor::Observer> observers_;

  base::WeakPtrFactory<BrightnessMonitorImpl> weak_ptr_factory_{this};
};

}  // namespace auto_screen_brightness
}  // namespace power
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_POWER_AUTO_SCREEN_BRIGHTNESS_BRIGHTNESS_MONITOR_IMPL_H_
