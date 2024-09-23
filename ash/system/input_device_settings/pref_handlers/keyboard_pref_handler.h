// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_INPUT_DEVICE_SETTINGS_PREF_HANDLERS_KEYBOARD_PREF_HANDLER_H_
#define ASH_SYSTEM_INPUT_DEVICE_SETTINGS_PREF_HANDLERS_KEYBOARD_PREF_HANDLER_H_

#include "ash/ash_export.h"
#include "ash/public/mojom/input_device_settings.mojom-forward.h"

class AccountId;
class PrefService;

namespace ash {

// Handles reading and updating prefs that store keyboard settings.
class ASH_EXPORT KeyboardPrefHandler {
 public:
  virtual ~KeyboardPrefHandler() = default;

  // Initializes device settings in prefs and update the `settings` member of
  // the `mojom::Keyboard` object. Respects all policies given by
  // `keyboard_policies`. If `pref_service` is null, sets the `settings` member
  // to default settings.
  virtual void InitializeKeyboardSettings(
      PrefService* pref_service,
      const mojom::KeyboardPolicies& keyboard_policies,
      mojom::Keyboard* keyboard) = 0;

  // Initializes login screen device settings using the passed in `keyboard`.
  // Settings will be stored either in `settings.keyboard.internal` or
  // `settings.keyboard.external` based on the value of `keyboard.is_external`.
  virtual void InitializeLoginScreenKeyboardSettings(
      PrefService* local_state,
      const AccountId& account_id,
      const mojom::KeyboardPolicies& keyboard_policies,
      mojom::Keyboard* keyboard) = 0;

  // Updates device settings stored in prefs to match the values in
  // `keyboard.settings`.
  virtual void UpdateKeyboardSettings(
      PrefService* pref_service,
      const mojom::KeyboardPolicies& keyboard_policies,
      const mojom::Keyboard& keyboard) = 0;

  // Updates login screen device settings stored in prefs to match the values in
  // `keyboard.settings`. Settings will be stored either in
  // `settings.keyboard.internal` or `settings.keyboard.external` based on the
  // value of `keyboard.is_external`.
  virtual void UpdateLoginScreenKeyboardSettings(
      PrefService* local_state,
      const AccountId& account_id,
      const mojom::KeyboardPolicies& keyboard_policies,
      const mojom::Keyboard& keyboard) = 0;

  // Updates the `settings` member of the `mojom::Keyboard` object using
  // default settings.
  virtual void InitializeWithDefaultKeyboardSettings(
      const mojom::KeyboardPolicies& keyboard_policies,
      mojom::Keyboard* keyboard) = 0;

  // Updates the default settings with the settings from the given keyboard.
  // These settings are applied to other ChromeOS keyboards that are connected
  // for the first time.
  virtual void UpdateDefaultChromeOSKeyboardSettings(
      PrefService* pref_service,
      const mojom::KeyboardPolicies& keyboard_policies,
      const mojom::Keyboard& keyboard) = 0;

  // Updates the default settings with the settings from the given keyboard.
  // These settings are applied to other Non-ChromeOS keyboards that are
  // connected for the first time.
  virtual void UpdateDefaultNonChromeOSKeyboardSettings(
      PrefService* pref_service,
      const mojom::KeyboardPolicies& keyboard_policies,
      const mojom::Keyboard& keyboard) = 0;

  // Updates the default settings with the settings from the given keyboard.
  // These settings are applied to other split modifier keyboards that are
  // connected for the first time.
  virtual void UpdateDefaultSplitModifierKeyboardSettings(
      PrefService* pref_service,
      const mojom::KeyboardPolicies& keyboard_policies,
      const mojom::Keyboard& keyboard) = 0;

  // Force refreshes the passed in keyboard settings to match the defaults for
  // the given `pref_service`.
  virtual void ForceInitializeWithDefaultSettings(
      PrefService* pref_service,
      const mojom::KeyboardPolicies& keyboard_policies,
      mojom::Keyboard* keyboard) = 0;
};

}  // namespace ash

#endif  // ASH_SYSTEM_INPUT_DEVICE_SETTINGS_PREF_HANDLERS_KEYBOARD_PREF_HANDLER_H_
