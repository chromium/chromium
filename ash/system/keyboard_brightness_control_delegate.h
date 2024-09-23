// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_KEYBOARD_BRIGHTNESS_CONTROL_DELEGATE_H_
#define ASH_SYSTEM_KEYBOARD_BRIGHTNESS_CONTROL_DELEGATE_H_

#include <optional>

#include "base/functional/callback_forward.h"

namespace ash {

// Enum to represent the source of a keyboard brightness change.
enum class KeyboardBrightnessChangeSource {
  kQuickSettings = 0,
  kSettingsApp = 1,
  kRestoredFromUserPref = 2,
  kMaxValue = kRestoredFromUserPref,
};

// Enum to represent the source of a keyboard ambient light sensor enabled
// change. Note that changing keyboard brightness can also disable the
// KeyboardAmbient Light Sensor. This change is not directly made by calling the
// HandleSetKeyboardAmbientLightSensorEnabled function in Chrome, it is handled
// in the platform.
enum class KeyboardAmbientLightSensorEnabledChangeSource {
  kSettingsApp = 0,
  kRestoredFromUserPref = 1,
  kSystemReenabled = 2,
  kMaxValue = kSystemReenabled,
};

// Delegate for controlling the keyboard brightness.
class KeyboardBrightnessControlDelegate {
 public:
  virtual ~KeyboardBrightnessControlDelegate() {}

  // Handles an accelerator-driven request to decrease or increase the keyboard
  // brightness.
  virtual void HandleKeyboardBrightnessDown() = 0;
  virtual void HandleKeyboardBrightnessUp() = 0;

  // Request that to turn keyboard brightness on or off.
  virtual void HandleToggleKeyboardBacklight() = 0;

  // Requests that the keyboard brightness be set to |percent|, in the range
  // [0.0, 100.0].  |gradual| specifies whether the transition to the new
  // brightness should be animated or instantaneous.
  virtual void HandleSetKeyboardBrightness(
      double percent,
      bool gradual,
      KeyboardBrightnessChangeSource source) = 0;

  // Asynchronously invokes |callback| with the current brightness, in the range
  // [0.0, 100.0]. In case of error, it is called with nullopt.
  virtual void HandleGetKeyboardBrightness(
      base::OnceCallback<void(std::optional<double>)> callback) = 0;

  // Sets whether the ambient light sensor should be used in keyboard brightness
  // calculations.
  virtual void HandleSetKeyboardAmbientLightSensorEnabled(
      bool enabled,
      KeyboardAmbientLightSensorEnabledChangeSource source) = 0;

  // Asynchronously invokes |callback| with the current keyboard ambient light
  // enabled status, In case of error, it is called with nullopt.
  virtual void HandleGetKeyboardAmbientLightSensorEnabled(
      base::OnceCallback<void(std::optional<bool>)> callback) = 0;
};

}  // namespace ash

#endif  // ASH_SYSTEM_KEYBOARD_BRIGHTNESS_CONTROL_DELEGATE_H_
