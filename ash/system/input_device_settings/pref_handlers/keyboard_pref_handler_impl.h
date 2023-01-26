// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_INPUT_DEVICE_SETTINGS_PREF_HANDLERS_KEYBOARD_PREF_HANDLER_IMPL_H_
#define ASH_SYSTEM_INPUT_DEVICE_SETTINGS_PREF_HANDLERS_KEYBOARD_PREF_HANDLER_IMPL_H_

#include "ash/ash_export.h"
#include "ash/system/input_device_settings/pref_handlers/keyboard_pref_handler.h"
#include "base/values.h"

class PrefService;

namespace ash {

class ASH_EXPORT KeyboardPrefHandlerImpl : public KeyboardPrefHandler {
 public:
  KeyboardPrefHandlerImpl();
  KeyboardPrefHandlerImpl(const KeyboardPrefHandlerImpl&) = delete;
  KeyboardPrefHandlerImpl& operator=(const KeyboardPrefHandlerImpl&) = delete;
  ~KeyboardPrefHandlerImpl() override;

  // KeyboardPrefHandler:
  void InitializeKeyboardSettings(PrefService* pref_service,
                                  mojom::Keyboard* keyboard) override;
  void UpdateKeyboardSettings(PrefService* pref_service,
                              const mojom::Keyboard& keyboard) override;

 private:
  mojom::KeyboardSettingsPtr GetNewKeyboardSettings(
      const mojom::Keyboard& keyboard);
  mojom::KeyboardSettingsPtr RetreiveKeyboardSettings(
      PrefService* prefs,
      const mojom::Keyboard& keyboard,
      const base::Value::Dict& settings_dict);
};

}  // namespace ash

#endif  // ASH_SYSTEM_INPUT_DEVICE_SETTINGS_PREF_HANDLERS_KEYBOARD_PREF_HANDLER_IMPL_H_
