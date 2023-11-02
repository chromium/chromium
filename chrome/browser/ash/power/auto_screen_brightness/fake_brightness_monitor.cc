// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/power/auto_screen_brightness/fake_brightness_monitor.h"

namespace ash {
namespace power {
namespace auto_screen_brightness {

FakeBrightnessMonitor::FakeBrightnessMonitor() {}

FakeBrightnessMonitor::~FakeBrightnessMonitor() = default;

void FakeBrightnessMonitor::ReportBrightnessMonitorInitialized() const {
  DCHECK_NE(brightness_monitor_status_, Status::kInitializing);
  const bool success = brightness_monitor_status_ == Status::kSuccess;
  for (auto& observer : observers_)
    observer.OnBrightnessMonitorInitialized(success);
}

void FakeBrightnessMonitor::ReportUserBrightnessChanged(
    const double old_brightness_percent,
    const double new_brightness_percent) const {
  for (auto& observer : observers_)
    observer.OnUserBrightnessChanged(old_brightness_percent,
                                     new_brightness_percent);
}

void FakeBrightnessMonitor::ReportUserBrightnessChangeRequested() const {
  for (auto& observer : observers_)
    observer.OnUserBrightnessChangeRequested();
}

void FakeBrightnessMonitor::AddObserver(Observer* const observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
  if (brightness_monitor_status_ != Status::kInitializing) {
    observer->OnBrightnessMonitorInitialized(brightness_monitor_status_ ==
                                             Status::kSuccess);
  }
}

void FakeBrightnessMonitor::RemoveObserver(Observer* const observer) {
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

}  // namespace auto_screen_brightness
}  // namespace power
}  // namespace ash
