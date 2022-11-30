// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POWER_AUTO_SCREEN_BRIGHTNESS_BRIGHTNESS_MONITOR_H_
#define CHROME_BROWSER_ASH_POWER_AUTO_SCREEN_BRIGHTNESS_BRIGHTNESS_MONITOR_H_

#include "base/observer_list_types.h"

namespace ash {
namespace power {
namespace auto_screen_brightness {

// Interface for monitoring the screen brightness.
class BrightnessMonitor {
 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class Status {
    kInitializing = 0,
    kSuccess = 1,
    kDisabled = 2,
    kMaxValue = kDisabled
  };

  class Observer : public base::CheckedObserver {
   public:
    Observer() = default;

    Observer(const Observer&) = delete;
    Observer& operator=(const Observer&) = delete;

    ~Observer() override = default;

    // Called when BrightnessMonitor is initialized.
    virtual void OnBrightnessMonitorInitialized(bool success) = 0;

    // Called soon after the screen brightness is changed in response to a user
    // request. Rapid changes are not reported; only the final change in a
    // sequence will be sent. The |old_brightness_percent| is the brightness
    // value just before user requested the change, and |new_brightness_percent|
    // is the final/consolidated brightness value after the change.
    virtual void OnUserBrightnessChanged(double old_brightness_percent,
                                         double new_brightness_percent) = 0;

    // Called for every user request, i.e. it's not consolidated like
    // |OnUserBrightnessChanged|.
    virtual void OnUserBrightnessChangeRequested() = 0;
  };

  virtual ~BrightnessMonitor() = default;

  // Adds or removes an observer.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;
};

}  // namespace auto_screen_brightness
}  // namespace power
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_POWER_AUTO_SCREEN_BRIGHTNESS_BRIGHTNESS_MONITOR_H_
