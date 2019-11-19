// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POWER_AUTO_SCREEN_BRIGHTNESS_BRIGHTNESS_MONITOR_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_POWER_AUTO_SCREEN_BRIGHTNESS_BRIGHTNESS_MONITOR_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "base/scoped_observer.h"
#include "base/sequenced_task_runner.h"
#include "base/task_runner_util.h"
#include "base/timer/timer.h"
#include "chrome/browser/chromeos/power/auto_screen_brightness/brightness_monitor.h"
#include "chromeos/dbus/power/power_manager_client.h"

namespace chromeos {
namespace power {
namespace auto_screen_brightness {

// Real implementation of BrightnessMonitor.
// It monitors user brightness changes and records the stabilized brightness.
class BrightnessMonitorImpl : public BrightnessMonitor,
                              public PowerManagerClient::Observer {
 public:
  // Once a user brightness adjustment is received, we wait for
  // |brightness_sample_delay_| to record the final brightness. It can be
  // configured from finch with default value set to |kBrightnessSampleDelay|.
  static constexpr base::TimeDelta kBrightnessSampleDelay =
      base::TimeDelta::FromSeconds(3);

  BrightnessMonitorImpl();
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
      base::Optional<double> brightness_percent);

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

  ScopedObserver<chromeos::PowerManagerClient,
                 chromeos::PowerManagerClient::Observer>
      power_manager_client_observer_{this};

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
  base::Optional<double> stable_brightness_percent_;
  // Current user selected brightness. It is reset after we've collected
  // final/stable user-requested brightness (i.e. after
  // |brightness_sample_timer_| times out).
  base::Optional<double> user_brightness_percent_;

  base::ObserverList<BrightnessMonitor::Observer> observers_;

  base::WeakPtrFactory<BrightnessMonitorImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(BrightnessMonitorImpl);
};

}  // namespace auto_screen_brightness
}  // namespace power
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_POWER_AUTO_SCREEN_BRIGHTNESS_BRIGHTNESS_MONITOR_IMPL_H_
