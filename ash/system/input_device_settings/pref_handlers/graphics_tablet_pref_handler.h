// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_INPUT_DEVICE_SETTINGS_PREF_HANDLERS_GRAPHICS_TABLET_PREF_HANDLER_H_
#define ASH_SYSTEM_INPUT_DEVICE_SETTINGS_PREF_HANDLERS_GRAPHICS_TABLET_PREF_HANDLER_H_

#include "ash/ash_export.h"
#include "ash/public/mojom/input_device_settings.mojom-forward.h"

class PrefService;

namespace ash {

// Handles reading and updating prefs that store graphics tablet settings.
class ASH_EXPORT GraphicsTabletPrefHandler {
 public:
  // Initializes device settings in prefs and update the `settings` member of
  // the `mojom::GraphicsTablet` object.
  virtual void InitializeGraphicsTabletSettings(
      PrefService* pref_service,
      mojom::GraphicsTablet* graphics_tablet) = 0;

  // Updates device settings stored in prefs to match the values in
  // `graphics_tablet.settings`.
  virtual void UpdateGraphicsTabletSettings(
      PrefService* pref_service,
      const mojom::GraphicsTablet& graphics_tablet) = 0;

 protected:
  virtual ~GraphicsTabletPrefHandler() = default;
};

}  // namespace ash

#endif  // ASH_SYSTEM_INPUT_DEVICE_SETTINGS_PREF_HANDLERS_GRAPHICS_GRAPHICS_TABLET_PREF_HANDLER_H_
