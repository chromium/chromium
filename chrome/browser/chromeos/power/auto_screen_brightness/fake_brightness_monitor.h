// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POWER_AUTO_SCREEN_BRIGHTNESS_FAKE_BRIGHTNESS_MONITOR_H_
#define CHROME_BROWSER_CHROMEOS_POWER_AUTO_SCREEN_BRIGHTNESS_FAKE_BRIGHTNESS_MONITOR_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/chromeos/power/auto_screen_brightness/brightness_monitor.h"

namespace chromeos {
namespace power {
namespace auto_screen_brightness {

// Fake BrightnessMonitor for testing only.
class FakeBrightnessMonitor : public BrightnessMonitor {
 public:
  FakeBrightnessMonitor();
  ~FakeBrightnessMonitor() override;

  void set_status(const Status status) { brightness_monitor_status_ = status; }

  // Calls its observers' OnBrightnessMonitorInitialized. Checks
  // |brightness_monitor_status_| is not kInitializing. Reported |success| is
  // true if |brightness_monitor_status_| is kSuccess, else it's false.
  void ReportBrightnessMonitorInitialized() const;

  // Calls its observers' OnUserBrightnessChanged.
  void ReportUserBrightnessChanged(double old_brightness_percent,
                                   double new_brightness_percent) const;

  // Calls its observers' OnUserBrightnessChangeRequested.
  void ReportUserBrightnessChangeRequested() const;

  // BrightnessMonitor overrides:
  void AddObserver(BrightnessMonitor::Observer* observer) override;
  void RemoveObserver(BrightnessMonitor::Observer* observer) override;

 private:
  BrightnessMonitor::Status brightness_monitor_status_ =
      BrightnessMonitor::Status::kInitializing;

  base::ObserverList<BrightnessMonitor::Observer> observers_;

  base::WeakPtrFactory<FakeBrightnessMonitor> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(FakeBrightnessMonitor);
};

}  // namespace auto_screen_brightness
}  // namespace power
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_POWER_AUTO_SCREEN_BRIGHTNESS_FAKE_BRIGHTNESS_MONITOR_H_
