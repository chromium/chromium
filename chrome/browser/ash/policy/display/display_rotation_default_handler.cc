// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/display/display_rotation_default_handler.h"

#include <utility>

#include "base/functional/callback_helpers.h"

namespace policy {

namespace {

display::Display::Rotation DisplayRotationFromRotationOptions(
    crosapi::mojom::DisplayRotationOptions option) {
  switch (option) {
    case crosapi::mojom::DisplayRotationOptions::kAutoRotate:
      // Auto rotation is ignored and considered as a 0-degrees rotation.
      return display::Display::ROTATE_0;

    case crosapi::mojom::DisplayRotationOptions::kZeroDegrees:
      return display::Display::ROTATE_0;

    case crosapi::mojom::DisplayRotationOptions::k90Degrees:
      return display::Display::ROTATE_90;

    case crosapi::mojom::DisplayRotationOptions::k180Degrees:
      return display::Display::ROTATE_180;

    case crosapi::mojom::DisplayRotationOptions::k270Degrees:
      return display::Display::ROTATE_270;
  }
}

crosapi::mojom::DisplayRotationOptions RotationOptionsFromDisplayRotation(
    display::Display::Rotation rotation) {
  switch (rotation) {
    case display::Display::ROTATE_0:
      return crosapi::mojom::DisplayRotationOptions::kZeroDegrees;

    case display::Display::ROTATE_90:
      return crosapi::mojom::DisplayRotationOptions::k90Degrees;

    case display::Display::ROTATE_180:
      return crosapi::mojom::DisplayRotationOptions::k180Degrees;

    case display::Display::ROTATE_270:
      return crosapi::mojom::DisplayRotationOptions::k270Degrees;
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
    crosapi::mojom::CrosDisplayConfigController* cros_display_config,
    const std::vector<crosapi::mojom::DisplayUnitInfoPtr>& info_list) {
  if (!policy_enabled_)
    return;
  for (const crosapi::mojom::DisplayUnitInfoPtr& display_unit_info :
       info_list) {
    std::string display_id = display_unit_info->id;

    if (rotated_display_ids_.find(display_id) != rotated_display_ids_.end())
      continue;

    rotated_display_ids_.insert(display_id);
    display::Display::Rotation rotation =
        DisplayRotationFromRotationOptions(display_unit_info->rotation_options);
    if (rotation == display_rotation_default_)
      continue;

    // The following sets only the |rotation| property of the display
    // configuration; no other properties will be affected.
    auto config_properties = crosapi::mojom::DisplayConfigProperties::New();
    config_properties->rotation = crosapi::mojom::DisplayRotation::New(
        RotationOptionsFromDisplayRotation(display_rotation_default_));
    cros_display_config->SetDisplayProperties(
        display_unit_info->id, std::move(config_properties),
        crosapi::mojom::DisplayConfigSource::kPolicy, base::DoNothing());
  }
}

}  // namespace policy
