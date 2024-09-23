// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_INPUT_DEVICE_SETTINGS_PREF_HANDLERS_MOUSE_PREF_HANDLER_H_
#define ASH_SYSTEM_INPUT_DEVICE_SETTINGS_PREF_HANDLERS_MOUSE_PREF_HANDLER_H_

#include "ash/ash_export.h"
#include "ash/public/mojom/input_device_settings.mojom-forward.h"
#include "ash/public/mojom/input_device_settings.mojom.h"

class AccountId;
class PrefService;

namespace ash {

// Handles reading and updating prefs that store mouse settings.
class ASH_EXPORT MousePrefHandler {
 public:
  virtual ~MousePrefHandler() = default;

  // Initializes device settings in prefs and update the `settings` member of
  // the `mojom::Mouse` object.
  // If `pref_service` is null, sets the `settings` member to default settings.
  virtual void InitializeMouseSettings(
      PrefService* pref_service,
      const mojom::MousePolicies& mouse_policies,
      mojom::Mouse* mouse) = 0;

  // Initializes login screen device settings using the passed in `mouse`.
  // Settings will be stored either in `settings.mouse.internal` or
  // `settings.mouse.external` based on the value of `mouse.is_external`.
  virtual void InitializeLoginScreenMouseSettings(
      PrefService* local_state,
      const AccountId& account_id,
      const mojom::MousePolicies& mouse_policies,
      mojom::Mouse* mouse) = 0;

  // Updates the `settings` member of the `mojom::Mouse` object using
  // default settings.
  virtual void InitializeWithDefaultMouseSettings(
      const mojom::MousePolicies& mouse_policies,
      mojom::Mouse* mouse) = 0;

  // Updates device settings stored in prefs to match the values in
  // `mouse.settings`.
  virtual void UpdateMouseSettings(PrefService* pref_service,
                                   const mojom::MousePolicies& mouse_policies,
                                   const mojom::Mouse& mouse) = 0;

  // Updates login screen device settings stored in prefs to match the values in
  // `mouse.settings`. Settings will be stored either in
  // `settings.mouse.internal` or `settings.mouse.external` based on the
  // value of `mouse.is_external`.
  virtual void UpdateLoginScreenMouseSettings(
      PrefService* local_state,
      const AccountId& account_id,
      const mojom::MousePolicies& mouse_policies,
      const mojom::Mouse& mouse) = 0;

  // Updates the default settings with the settings from the given mouse. These
  // settings are applied to other mice that are connected for the first time.
  virtual void UpdateDefaultMouseSettings(
      PrefService* pref_service,
      const mojom::MousePolicies& mouse_policies,
      const mojom::Mouse& mouse) = 0;

  // Force refreshes the passed in mouse settings to match the defaults for the
  // given `pref_service`.
  virtual void ForceInitializeWithDefaultSettings(
      PrefService* pref_service,
      const mojom::MousePolicies& mouse_policies,
      mojom::Mouse* mouse) = 0;
};

}  // namespace ash

#endif  // ASH_SYSTEM_INPUT_DEVICE_SETTINGS_PREF_HANDLERS_MOUSE_PREF_HANDLER_H_
