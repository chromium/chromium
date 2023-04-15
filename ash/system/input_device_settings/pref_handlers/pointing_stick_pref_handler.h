// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_INPUT_DEVICE_SETTINGS_PREF_HANDLERS_POINTING_STICK_PREF_HANDLER_H_
#define ASH_SYSTEM_INPUT_DEVICE_SETTINGS_PREF_HANDLERS_POINTING_STICK_PREF_HANDLER_H_

#include "ash/ash_export.h"
#include "ash/public/mojom/input_device_settings.mojom-forward.h"

class AccountId;
class PrefService;

namespace ash {

// Handles reading and updating prefs that store pointing stick settings.
class ASH_EXPORT PointingStickPrefHandler {
 public:
  virtual ~PointingStickPrefHandler() = default;

  // Initializes device settings in prefs and update the `settings` member of
  // the `mojom::PointingStick` object.
  // If `pref_service` is null, sets the `settings` member to default settings.
  virtual void InitializePointingStickSettings(
      PrefService* pref_service,
      mojom::PointingStick* pointing_stick) = 0;

  // Initializes login screen device settings using the passed in
  // `pointing_stick`. Settings will be stored either in
  // `settings.pointing_stick.internal` or `settings.pointing_stick.external`
  // based on the value of `pointing_stick.is_external`.
  virtual void InitializeLoginScreenPointingStickSettings(
      PrefService* local_state,
      const AccountId& account_id,
      mojom::PointingStick* pointing_stick) = 0;

  // Updates the `settings` member of the `mojom::PointingStick` object using
  // default settings.
  virtual void InitializeWithDefaultPointingStickSettings(
      mojom::PointingStick* pointing_stick) = 0;

  // Updates device settings stored in prefs to match the values in
  // `pointing_stick.settings`.
  virtual void UpdatePointingStickSettings(
      PrefService* pref_service,
      const mojom::PointingStick& pointing_stick) = 0;

  // Updates login screen device settings stored in prefs to match the values
  // in `pointing_stick.settings`. Settings will be stored either in
  // `settings.pointing_stick.internal` or `settings.pointing_stick.external`
  // based on the value of `pointing_stick.is_external`.
  virtual void UpdateLoginScreenPointingStickSettings(
      PrefService* local_state,
      const AccountId& account_id,
      const mojom::PointingStick& pointing_stick) = 0;
};

}  // namespace ash

#endif  // ASH_SYSTEM_INPUT_DEVICE_SETTINGS_PREF_HANDLERS_POINTING_STICK_PREF_HANDLER_H_
