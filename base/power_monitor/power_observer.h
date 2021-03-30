// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_POWER_MONITOR_POWER_OBSERVER_H_
#define BASE_POWER_MONITOR_POWER_OBSERVER_H_

#include "base/base_export.h"
#include "base/compiler_specific.h"

namespace base {

class BASE_EXPORT PowerSuspendObserver {
 public:
  // Notification that the system is suspending.
  virtual void OnSuspend() {}

  // Notification that the system is resuming.
  virtual void OnResume() {}

 protected:
  virtual ~PowerSuspendObserver() = default;
};

class BASE_EXPORT PowerStateObserver {
 public:
  // Notification of a change in power status of the computer, such
  // as from switching between battery and A/C power.
  virtual void OnPowerStateChange(bool on_battery_power) = 0;

 protected:
  virtual ~PowerStateObserver() = default;
};

class BASE_EXPORT PowerThermalObserver {
 public:
  // Values to indicate the system's thermal states: from kNominal onwards to
  // kCritical they represent increasing SoC die temperatures, usually needing
  // disruptive actions by the system like e.g. turning on the fans (on systems
  // equipped with those) or reducing voltage and frequency (oftentimes
  // degrading overall responsiveness). The taxonomy is derived from MacOS (see
  // e.g. [1]) but applies to others e.g. Linux/ChromeOS.
  // [1] https://developer.apple.com/library/archive/documentation/Performance/Conceptual/power_efficiency_guidelines_osx/RespondToThermalStateChanges.html
  enum class DeviceThermalState {
    kUnknown,
    kNominal,
    kFair,
    kSerious,
    kCritical,
  };

  // Notification of a change in the thermal status of the system, such as
  // entering a critical temperature range. Depending on the severity, the SoC
  // or the OS might take steps to reduce said temperature e.g., throttling the
  // CPU or switching on the fans if available. API clients may react to the new
  // state by reducing expensive computing tasks (e.g. video encoding), or
  // notifying the user. The same |new_state| might be received repeatedly.
  // TODO(crbug.com/1071431): implemented on MacOS, extend to Linux/CrOs.
  virtual void OnThermalStateChange(DeviceThermalState new_state) = 0;

 protected:
  virtual ~PowerThermalObserver() = default;
};

}  // namespace base

#endif  // BASE_POWER_MONITOR_POWER_OBSERVER_H_
