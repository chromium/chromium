// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/system_display/display_info_provider_utils.h"

#include "ash/display/cros_display_config.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "chromeos/crosapi/mojom/cros_display_config.mojom.h"
#include "ui/display/display.h"
#include "ui/display/display_layout.h"
#include "ui/display/screen.h"
#include "ui/display/types/display_constants.h"

namespace extensions {

namespace {
namespace system_display = api::system_display;
}  // namespace

int64_t GetDisplayId(const std::string& display_id_str) {
  int64_t display_id;
  if (!base::StringToInt64(display_id_str, &display_id)) {
    display_id = display::kInvalidDisplayId;
  }
  return display_id;
}

display::Display GetDisplayForId(const std::string& display_id_str) {
  int64_t id = GetDisplayId(display_id_str);
  display::Display display;
  display::Screen::Get()->GetDisplayWithDisplayId(id, &display);
  return display;
}

display::DisplayPlacement::Position GetDisplayLayoutPosition(
    system_display::LayoutPosition position) {
  switch (position) {
    case system_display::LayoutPosition::kTop:
      return display::DisplayPlacement::TOP;
    case system_display::LayoutPosition::kRight:
      return display::DisplayPlacement::RIGHT;
    case system_display::LayoutPosition::kBottom:
      return display::DisplayPlacement::BOTTOM;
    case system_display::LayoutPosition::kLeft:
    case system_display::LayoutPosition::kNone:
      return display::DisplayPlacement::LEFT;
  }
  NOTREACHED();
}

gfx::Insets GetInsets(const system_display::Insets& insets) {
  return gfx::Insets::TLBR(insets.top, insets.left, insets.bottom,
                           insets.right);
}

bool IsValidRotation(int rotation) {
  return rotation == -1 || rotation == 0 || rotation == 90 || rotation == 180 ||
         rotation == 270;
}

crosapi::mojom::DisplayRotationOptions GetMojomDisplayRotationOptions(
    int rotation_value) {
  DCHECK(IsValidRotation(rotation_value));

  switch (rotation_value) {
    case -1:
      return crosapi::mojom::DisplayRotationOptions::kAutoRotate;
    case 0:
      return crosapi::mojom::DisplayRotationOptions::kZeroDegrees;
    case 90:
      return crosapi::mojom::DisplayRotationOptions::k90Degrees;
    case 180:
      return crosapi::mojom::DisplayRotationOptions::k180Degrees;
    case 270:
      return crosapi::mojom::DisplayRotationOptions::k270Degrees;
    default:
      NOTREACHED();
  }
}

int GetRotationFromMojomDisplayRotationInfo(
    crosapi::mojom::DisplayRotationOptions rotation_options) {
  switch (rotation_options) {
    case crosapi::mojom::DisplayRotationOptions::kAutoRotate:
      return -1;
    case crosapi::mojom::DisplayRotationOptions::kZeroDegrees:
      return 0;
    case crosapi::mojom::DisplayRotationOptions::k90Degrees:
      return 90;
    case crosapi::mojom::DisplayRotationOptions::k180Degrees:
      return 180;
    case crosapi::mojom::DisplayRotationOptions::k270Degrees:
      return 270;
  }
}

std::optional<std::string> ValidateDisplayPropertiesInput(
    const std::string& display_id_str,
    const system_display::DisplayProperties& info) {
  int64_t id = GetDisplayId(display_id_str);
  if (id == display::kInvalidDisplayId) {
    return "Invalid display id";
  }

  const display::Display& primary = display::Screen::Get()->GetPrimaryDisplay();
  bool is_primary = id == primary.id() || (info.is_primary && *info.is_primary);

  if (info.is_unified) {
    if (!is_primary) {
      return "Unified desktop mode can only be set for the primary display.";
    }
    // Setting isUnfied may change the display layout so no other properties
    // should be set.
    if (info.mirroring_source_id) {
      return "Unified desktop mode can not be set with mirroringSourceId.";
    }
    if (info.bounds_origin_x || info.bounds_origin_y || info.rotation ||
        info.overscan || info.display_mode || info.display_zoom_factor) {
      LOG(WARNING)
          << "Unified mode set with other properties which will be ignored.";
    }
    return std::nullopt;
  }

  // If mirroring source parameter is specified, no other properties should be
  // set the display list may change when mirroring is applied.
  if (info.mirroring_source_id &&
      (info.is_primary || info.bounds_origin_x || info.bounds_origin_y ||
       info.rotation || info.overscan || info.display_mode ||
       info.display_zoom_factor)) {
    return "No other parameter should be set with mirroringSourceId.";
  }

  // Verify the rotation value is valid.
  if (info.rotation && !IsValidRotation(*info.rotation)) {
    return "Invalid rotation.";
  }

  return std::nullopt;
}

system_display::DisplayMode GetDisplayModeFromUi(
    const display::ManagedDisplayMode& mode) {
  gfx::Size native_size = mode.size();
  gfx::Size dip_size = mode.GetSizeInDIP();
  system_display::DisplayMode result;
  result.width = dip_size.width();
  result.height = dip_size.height();
  result.width_in_native_pixels = native_size.width();
  result.height_in_native_pixels = native_size.height();
  result.device_scale_factor = mode.device_scale_factor();
  result.refresh_rate = mode.refresh_rate();
  result.is_native = mode.native();
  result.is_interlaced = mode.is_interlaced();
  return result;
}

system_display::DisplayUnitInfo GetDisplayUnitInfoFromAsh(
    const ash::DisplayUnitInfo& ash_info) {
  system_display::DisplayUnitInfo info;
  info.id = base::NumberToString(ash_info.id);
  info.name = ash_info.name;
  if (ash_info.edid) {
    info.edid.emplace();
    info.edid->manufacturer_id = ash_info.edid->manufacturer_id;
    info.edid->product_id = ash_info.edid->product_id;
    info.edid->year_of_manufacture = ash_info.edid->year_of_manufacture;
  }
  info.is_primary = ash_info.is_primary;
  info.is_internal = ash_info.is_internal;
  info.active_state = ash_info.is_detected
                          ? system_display::ActiveState::kActive
                          : system_display::ActiveState::kInactive;
  info.is_enabled = ash_info.is_enabled;
  info.is_auto_rotation_allowed = ash_info.is_auto_rotation_allowed;
  info.dpi_x = ash_info.dpi_x;
  info.dpi_y = ash_info.dpi_y;
  info.rotation =
      GetRotationFromMojomDisplayRotationInfo(ash_info.rotation_options);
  const gfx::Rect& bounds = ash_info.bounds;
  info.bounds.left = bounds.x();
  info.bounds.top = bounds.y();
  info.bounds.width = bounds.width();
  info.bounds.height = bounds.height();
  const gfx::Insets& overscan = ash_info.overscan;
  info.overscan.left = overscan.left();
  info.overscan.top = overscan.top();
  info.overscan.right = overscan.right();
  info.overscan.bottom = overscan.bottom();
  const gfx::Rect& work_area = ash_info.work_area;
  info.work_area.left = work_area.x();
  info.work_area.top = work_area.y();
  info.work_area.width = work_area.width();
  info.work_area.height = work_area.height();
  for (const auto& mode : ash_info.available_display_modes) {
    info.modes.push_back(GetDisplayModeFromUi(mode));
  }
  if (!info.modes.empty()) {
    int index = ash_info.selected_display_mode_index;
    if (index < 0 || index >= static_cast<int>(info.modes.size())) {
      index = 0;
    }
    info.modes[index].is_selected = true;
  }
  info.has_touch_support = ash_info.has_touch_support;
  info.has_accelerometer_support = ash_info.has_accelerometer_support;
  info.available_display_zoom_factors = ash_info.available_display_zoom_factors;
  info.display_zoom_factor = ash_info.display_zoom_factor;
  return info;
}

display::TouchCalibrationData::CalibrationPointPair GetTouchCalibrationPair(
    const system_display::TouchCalibrationPair& pair) {
  return {gfx::Point(pair.display_point.x, pair.display_point.y),
          gfx::Point(pair.touch_point.x, pair.touch_point.y)};
}

void SetDisplayUnitInfoLayoutProperties(
    const ash::DisplayLayoutInfo& layout,
    system_display::DisplayUnitInfo* display) {
  display->is_unified = layout.layout_mode == ash::DisplayLayoutMode::kUnified;
  if (layout.mirror_source_id.has_value()) {
    display->mirroring_source_id =
        base::NumberToString(*layout.mirror_source_id);
    if (layout.mirror_destination_ids.has_value()) {
      for (int64_t id : *layout.mirror_destination_ids) {
        display->mirroring_destination_ids.push_back(base::NumberToString(id));
      }
    }
  }
}

}  // namespace extensions
