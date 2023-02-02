// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_INPUT_DEVICE_SETTINGS_PREF_HANDLERS_TOUCHPAD_PREF_HANDLER_H_
#define ASH_SYSTEM_INPUT_DEVICE_SETTINGS_PREF_HANDLERS_TOUCHPAD_PREF_HANDLER_H_

#include "ash/ash_export.h"
#include "ash/public/mojom/input_device_settings.mojom-forward.h"

class PrefService;

namespace ash {

// Handles reading and updating prefs that store touchpad settings.
class ASH_EXPORT TouchpadPrefHandler {
 public:
  // Initializes device settings in prefs and update the `settings` member of
  // the `mojom::Touchpad` object.
  virtual void InitializeTouchpadSettings(PrefService* pref_service,
                                          mojom::Touchpad* touchpad) = 0;

  // Updates device settings stored in prefs to match the values in
  // `touchpad.settings`.
  virtual void UpdateTouchpadSettings(PrefService* pref_service,
                                      const mojom::Touchpad& touchpad) = 0;

 protected:
  virtual ~TouchpadPrefHandler() = default;
};

}  // namespace ash

#endif  // ASH_SYSTEM_INPUT_DEVICE_SETTINGS_PREF_HANDLERS_TOUCHPAD_PREF_HANDLER_H_
