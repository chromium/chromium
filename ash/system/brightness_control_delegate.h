// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_BRIGHTNESS_CONTROL_DELEGATE_H_
#define ASH_SYSTEM_BRIGHTNESS_CONTROL_DELEGATE_H_

#include <optional>

#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"

namespace ash {

// Delegate for controlling the brightness.
class BrightnessControlDelegate {
 public:
  virtual ~BrightnessControlDelegate() {}

  // Handles an accelerator-driven request to decrease or increase the screen
  // brightness.
  virtual void HandleBrightnessDown() = 0;
  virtual void HandleBrightnessUp() = 0;

  // Enum to represent the source of a brightness change, i.e. what triggered
  // the brightness change.
  enum class BrightnessChangeSource {
    kUnknown = 0,
    kQuickSettings = 1,
    kSettingsApp = 2,
    kRestoredFromUserPref = 3,
    kMaxValue = kRestoredFromUserPref,
  };

  // Enum to represent the source of a ambient light sensor change,
  // Note that changing brightness can also disable the Ambient Light
  // Sensor. This change is not directly made by calling the
  // SetAmbientLightSensorEnabled function in the BrightnessControlDelegate in
  // Chrome, it is handled in the platform.
  enum class AmbientLightSensorEnabledChangeSource {
    kSettingsApp = 0,
    kRestoredFromUserPref = 1,
    kSystemReenabled = 2,
    kMaxValue = kSystemReenabled,
  };

  // Requests that the brightness be set to |percent|, in the range
  // [0.0, 100.0].  |gradual| specifies whether the transition to the new
  // brightness should be animated or instantaneous. |source| is required to
  // indicate what is causing this brightness change.
  virtual void SetBrightnessPercent(double percent,
                                    bool gradual,
                                    BrightnessChangeSource source) = 0;

  // Asynchronously invokes |callback| with the current brightness, in the range
  // [0.0, 100.0]. In case of error, it is called with nullopt.
  virtual void GetBrightnessPercent(
      base::OnceCallback<void(std::optional<double>)> callback) = 0;

  // Sets whether the ambient light sensor should be used in brightness
  // calculations.
  virtual void SetAmbientLightSensorEnabled(
      bool enabled,
      AmbientLightSensorEnabledChangeSource source) = 0;

  // Asynchronously invokes |callback| with true if the ambient light sensor is
  // enabled (i.e. if the ambient light sensor is currently being used in
  // brightness calculations). In case of error, |callback| will be run with
  // nullopt.
  virtual void GetAmbientLightSensorEnabled(
      base::OnceCallback<void(std::optional<bool>)> callback) = 0;

  // Asynchronously invokes |callback| with true if the device has at least one
  // ambient light sensor. In case of error, |callback| will be run with
  // nullopt.
  virtual void HasAmbientLightSensor(
      base::OnceCallback<void(std::optional<bool>)> callback) = 0;
};

}  // namespace ash

#endif  // ASH_SYSTEM_BRIGHTNESS_CONTROL_DELEGATE_H_
