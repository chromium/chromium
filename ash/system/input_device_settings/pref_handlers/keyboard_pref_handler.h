// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_INPUT_DEVICE_SETTINGS_PREF_HANDLERS_KEYBOARD_PREF_HANDLER_H_
#define ASH_SYSTEM_INPUT_DEVICE_SETTINGS_PREF_HANDLERS_KEYBOARD_PREF_HANDLER_H_

#include "ash/ash_export.h"
#include "ash/public/mojom/input_device_settings.mojom-forward.h"

class PrefService;

namespace ash {

// Handles reading and updating prefs that store keyboard settings.
class ASH_EXPORT KeyboardPrefHandler {
 public:
  virtual ~KeyboardPrefHandler() = default;

  // Initializes device settings in prefs and update the `settings` member of
  // the `mojom::Keyboard` object.
  // If `pref_service` is null, sets the `settings` member to default settings.
  virtual void InitializeKeyboardSettings(PrefService* pref_service,
                                          mojom::Keyboard* keyboard) = 0;

  // Updates device settings stored in prefs to match the values in
  // `keyboard.settings`.
  virtual void UpdateKeyboardSettings(PrefService* pref_service,
                                      const mojom::Keyboard& keyboard) = 0;
};

}  // namespace ash

#endif  // ASH_SYSTEM_INPUT_DEVICE_SETTINGS_PREF_HANDLERS_KEYBOARD_PREF_HANDLER_H_
