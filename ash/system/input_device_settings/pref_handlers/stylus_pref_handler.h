// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_INPUT_DEVICE_SETTINGS_PREF_HANDLERS_STYLUS_PREF_HANDLER_H_
#define ASH_SYSTEM_INPUT_DEVICE_SETTINGS_PREF_HANDLERS_STYLUS_PREF_HANDLER_H_

#include "ash/ash_export.h"
#include "ash/public/mojom/input_device_settings.mojom-forward.h"

class PrefService;

namespace ash {

// Handles reading and updating prefs that store stylus settings.
class ASH_EXPORT StylusPrefHandler {
 public:
  // Initializes device settings in prefs and update the `settings` member of
  // the `mojom::Stylus` object.
  virtual void InitializeStylusSettings(PrefService* pref_service,
                                        mojom::Stylus* stylus) = 0;

  // Updates device settings stored in prefs to match the values in
  // `stylus.settings`.
  virtual void UpdateStylusSettings(PrefService* pref_service,
                                    const mojom::Stylus& stylus) = 0;

 protected:
  virtual ~StylusPrefHandler() = default;
};

}  // namespace ash

#endif  // ASH_SYSTEM_INPUT_DEVICE_SETTINGS_PREF_HANDLERS_STYLUS_PREF_HANDLER_H_
