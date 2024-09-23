// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_INPUT_DEVICE_SETTINGS_PREF_HANDLERS_KEYBOARD_PREF_HANDLER_IMPL_H_
#define ASH_SYSTEM_INPUT_DEVICE_SETTINGS_PREF_HANDLERS_KEYBOARD_PREF_HANDLER_IMPL_H_

#include "ash/ash_export.h"
#include "ash/public/mojom/input_device_settings.mojom-forward.h"
#include "ash/system/input_device_settings/pref_handlers/keyboard_pref_handler.h"
#include "base/values.h"

class AccountId;
class PrefService;

namespace ash {

class ASH_EXPORT KeyboardPrefHandlerImpl : public KeyboardPrefHandler {
 public:
  KeyboardPrefHandlerImpl();
  KeyboardPrefHandlerImpl(const KeyboardPrefHandlerImpl&) = delete;
  KeyboardPrefHandlerImpl& operator=(const KeyboardPrefHandlerImpl&) = delete;
  ~KeyboardPrefHandlerImpl() override;

  // KeyboardPrefHandler:
  void InitializeKeyboardSettings(
      PrefService* pref_service,
      const mojom::KeyboardPolicies& keyboard_policies,
      mojom::Keyboard* keyboard) override;

  void InitializeLoginScreenKeyboardSettings(
      PrefService* local_state,
      const AccountId& account_id,
      const mojom::KeyboardPolicies& keyboard_policies,
      mojom::Keyboard* keyboard) override;

  // Updates device settings stored in prefs to match the values in
  // `keyboard.settings`.
  void UpdateKeyboardSettings(PrefService* pref_service,
                              const mojom::KeyboardPolicies& keyboard_policies,
                              const mojom::Keyboard& keyboard) override;

  void UpdateLoginScreenKeyboardSettings(
      PrefService* local_state,
      const AccountId& account_id,
      const mojom::KeyboardPolicies& keyboard_policies,
      const mojom::Keyboard& keyboard) override;

  void InitializeWithDefaultKeyboardSettings(
      const mojom::KeyboardPolicies& keyboard_policies,
      mojom::Keyboard* keyboard) override;

  void UpdateDefaultChromeOSKeyboardSettings(
      PrefService* pref_service,
      const mojom::KeyboardPolicies& keyboard_policies,
      const mojom::Keyboard& keyboard) override;

  void UpdateDefaultNonChromeOSKeyboardSettings(
      PrefService* pref_service,
      const mojom::KeyboardPolicies& keyboard_policies,
      const mojom::Keyboard& keyboard) override;

  void UpdateDefaultSplitModifierKeyboardSettings(
      PrefService* pref_service,
      const mojom::KeyboardPolicies& keyboard_policies,
      const mojom::Keyboard& keyboard) override;

  void ForceInitializeWithDefaultSettings(
      PrefService* pref_service,
      const mojom::KeyboardPolicies& keyboard_policies,
      mojom::Keyboard* keyboard) override;
};

}  // namespace ash

#endif  // ASH_SYSTEM_INPUT_DEVICE_SETTINGS_PREF_HANDLERS_KEYBOARD_PREF_HANDLER_IMPL_H_
