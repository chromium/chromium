// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_INPUT_DEVICE_SETTINGS_PREF_HANDLERS_TOUCHPAD_PREF_HANDLER_H_
#define ASH_SYSTEM_INPUT_DEVICE_SETTINGS_PREF_HANDLERS_TOUCHPAD_PREF_HANDLER_H_

#include "ash/ash_export.h"
#include "ash/public/mojom/input_device_settings.mojom-forward.h"

class AccountId;
class PrefService;

namespace ash {

// Handles reading and updating prefs that store touchpad settings.
class ASH_EXPORT TouchpadPrefHandler {
 public:
  virtual ~TouchpadPrefHandler() = default;

  // Initializes device settings in prefs and update the `settings` member of
  // the `mojom::Touchpad` object.
  // If `pref_service` is null, sets the `settings` member to default settings.
  virtual void InitializeTouchpadSettings(PrefService* pref_service,
                                          mojom::Touchpad* touchpad) = 0;

  // Initializes login screen device settings using the passed in `touchpad`.
  // Settings will be stored either in `settings.touchpad.internal` or
  // `settings.touchpad.external` based on the value of `touchpad.is_external`.
  virtual void InitializeLoginScreenTouchpadSettings(
      PrefService* local_state,
      const AccountId& account_id,
      mojom::Touchpad* touchpad) = 0;

  // Updates the `settings` member of the `mojom::Touchpad` object using
  // default settings.
  virtual void InitializeWithDefaultTouchpadSettings(
      mojom::Touchpad* touchpad) = 0;

  // Updates device settings stored in prefs to match the values in
  // `touchpad.settings`.
  virtual void UpdateTouchpadSettings(PrefService* pref_service,
                                      const mojom::Touchpad& touchpad) = 0;

  // Updates login screen device settings stored in prefs to match the values
  // in `touchpad.settings`. Settings will be stored either in
  // `settings.touchpad.internal` or `settings.touchpad.external`
  // based on the value of `touchpad.is_external`.
  virtual void UpdateLoginScreenTouchpadSettings(
      PrefService* local_state,
      const AccountId& account_id,
      const mojom::Touchpad& touchpad) = 0;

  virtual void UpdateDefaultTouchpadSettings(
      PrefService* pref_service,
      const mojom::Touchpad& touchpad) = 0;

  // Force refreshes the passed in touchpad settings to match the defaults for
  // the given `pref_service`.
  virtual void ForceInitializeWithDefaultSettings(
      PrefService* pref_service,
      mojom::Touchpad* touchpad) = 0;
};

}  // namespace ash

#endif  // ASH_SYSTEM_INPUT_DEVICE_SETTINGS_PREF_HANDLERS_TOUCHPAD_PREF_HANDLER_H_
