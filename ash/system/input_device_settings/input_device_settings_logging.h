// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_SETTINGS_LOGGING_H_
#define ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_SETTINGS_LOGGING_H_

#include <sstream>
#include <string_view>

#include "ash/ash_export.h"
#include "ash/public/mojom/input_device_settings.mojom.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "components/peripherals/logging/logging.h"

namespace ash {

// Get settings log for graphics tablet.
ASH_EXPORT std::string GetGraphicsTabletSettingsLog(
    std::string_view category,
    const mojom::GraphicsTablet& graphics_tablet);

// Get settings log for keyboard.
ASH_EXPORT std::string GetKeyboardSettingsLog(std::string_view category,
                                              const mojom::Keyboard& keyboard);

// Get settings log for mouse.
ASH_EXPORT std::string GetMouseSettingsLog(std::string_view category,
                                           const mojom::Mouse& mouse);

// Get settings log for pointing stick.
ASH_EXPORT std::string GetPointingStickSettingsLog(
    std::string_view category,
    const mojom::PointingStick& pointing_stick);

// Get settings log for touchpad.
ASH_EXPORT std::string GetTouchpadSettingsLog(std::string_view category,
                                              const mojom::Touchpad& touchpad);

}  // namespace ash

#endif  // ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_SETTINGS_LOGGING_H_
