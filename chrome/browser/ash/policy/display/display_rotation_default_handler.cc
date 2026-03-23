// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/display/display_rotation_default_handler.h"

#include <utility>

#include "ash/display/cros_display_config.h"
#include "ash/shell.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/string_number_conversions.h"

namespace policy {

namespace {

display::Display::Rotation DisplayRotationFromRotationOptions(
    ash::DisplayRotationOptions option) {
  switch (option) {
    case ash::DisplayRotationOptions::kAutoRotate:
      // Auto rotation is ignored and considered as a 0-degrees rotation.
      return display::Display::ROTATE_0;

    case ash::DisplayRotationOptions::kZeroDegrees:
      return display::Display::ROTATE_0;

    case ash::DisplayRotationOptions::k90Degrees:
      return display::Display::ROTATE_90;

    case ash::DisplayRotationOptions::k180Degrees:
      return display::Display::ROTATE_180;

    case ash::DisplayRotationOptions::k270Degrees:
      return display::Display::ROTATE_270;
  }
}

ash::DisplayRotationOptions RotationOptionsFromDisplayRotation(
    display::Display::Rotation rotation) {
  switch (rotation) {
    case display::Display::ROTATE_0:
      return ash::DisplayRotationOptions::kZeroDegrees;

    case display::Display::ROTATE_90:
      return ash::DisplayRotationOptions::k90Degrees;

    case display::Display::ROTATE_180:
      return ash::DisplayRotationOptions::k180Degrees;

    case display::Display::ROTATE_270:
      return ash::DisplayRotationOptions::k270Degrees;
  }
}

}  // namespace

DisplayRotationDefaultHandler::DisplayRotationDefaultHandler() = default;

DisplayRotationDefaultHandler::~DisplayRotationDefaultHandler() = default;

const char* DisplayRotationDefaultHandler::SettingName() {
  return ash::kDisplayRotationDefault;
}

// Reads |ash::kDisplayRotationDefault| from CrosSettings and stores
// its value, and whether it has a value, in member variables
// |display_rotation_default_| and |policy_enabled_|.
void DisplayRotationDefaultHandler::OnSettingUpdate() {
  int new_rotation;
  bool new_policy_enabled = ash::CrosSettings::Get()->GetInteger(
      ash::kDisplayRotationDefault, &new_rotation);
  display::Display::Rotation new_display_rotation_default =
      display::Display::ROTATE_0;
  if (new_policy_enabled) {
    if (new_rotation >= display::Display::ROTATE_0 &&
        new_rotation <= display::Display::ROTATE_270) {
      new_display_rotation_default =
          static_cast<display::Display::Rotation>(new_rotation);
    } else {
      LOG(ERROR) << "CrosSettings contains invalid value " << new_rotation
                 << " for DisplayRotationDefault. Ignoring setting.";
      new_policy_enabled = false;
    }
  }
  if (new_policy_enabled != policy_enabled_ ||
      (new_policy_enabled &&
       new_display_rotation_default != display_rotation_default_)) {
    policy_enabled_ = new_policy_enabled;
    display_rotation_default_ = new_display_rotation_default;
    rotated_display_ids_.clear();
  }
}

void DisplayRotationDefaultHandler::ApplyChanges(
    ash::CrosDisplayConfig& cros_display_config,
    const std::vector<ash::DisplayUnitInfo>& info_list) {
  if (!policy_enabled_)
    return;
  for (const auto& display_unit_info : info_list) {
    if (rotated_display_ids_.find(display_unit_info.id) !=
        rotated_display_ids_.end()) {
      continue;
    }

    rotated_display_ids_.insert(display_unit_info.id);
    display::Display::Rotation rotation =
        DisplayRotationFromRotationOptions(display_unit_info.rotation_options);
    if (rotation == display_rotation_default_)
      continue;

    // The following sets only the |rotation| property of the display
    // configuration; no other properties will be affected.
    ash::DisplayConfigProperties config_properties;
    config_properties.rotation =
        RotationOptionsFromDisplayRotation(display_rotation_default_);
    cros_display_config.SetDisplayProperties(
        base::NumberToString(display_unit_info.id), config_properties,
        ash::DisplayConfigSource::kPolicy);
  }
}

}  // namespace policy
