// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/system_display/display_info_provider_utils.h"

#include "base/strings/string_number_conversions.h"
#include "chromeos/crosapi/mojom/cros_display_config.mojom.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/display/types/display_constants.h"

namespace extensions {

namespace {

namespace system_display = api::system_display;

system_display::LayoutPosition GetLayoutPositionFromMojo(
    crosapi::mojom::DisplayLayoutPosition position) {
  switch (position) {
    case crosapi::mojom::DisplayLayoutPosition::kTop:
      return system_display::LayoutPosition::kTop;
    case crosapi::mojom::DisplayLayoutPosition::kRight:
      return system_display::LayoutPosition::kRight;
    case crosapi::mojom::DisplayLayoutPosition::kBottom:
      return system_display::LayoutPosition::kBottom;
    case crosapi::mojom::DisplayLayoutPosition::kLeft:
      return system_display::LayoutPosition::kLeft;
  }
  NOTREACHED_IN_MIGRATION();
  return system_display::LayoutPosition::kLeft;
}
}  // namespace

void OnGetDisplayLayoutResult(
    base::OnceCallback<void(DisplayInfoProvider::DisplayLayoutList)> callback,
    crosapi::mojom::DisplayLayoutInfoPtr info) {
  DisplayInfoProvider::DisplayLayoutList result;
  if (info->layouts) {
    for (crosapi::mojom::DisplayLayoutPtr& layout : *info->layouts) {
      api::system_display::DisplayLayout display_layout;
      display_layout.id = layout->id;
      display_layout.parent_id = layout->parent_id;
      display_layout.position = GetLayoutPositionFromMojo(layout->position);
      display_layout.offset = layout->offset;
      result.emplace_back(std::move(display_layout));
    }
  }
  std::move(callback).Run(std::move(result));
}

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
  display::Screen::GetScreen()->GetDisplayWithDisplayId(id, &display);
  return display;
}

crosapi::mojom::DisplayLayoutPosition GetDisplayLayoutPosition(
    system_display::LayoutPosition position) {
  switch (position) {
    case system_display::LayoutPosition::kTop:
      return crosapi::mojom::DisplayLayoutPosition::kTop;
    case system_display::LayoutPosition::kRight:
      return crosapi::mojom::DisplayLayoutPosition::kRight;
    case system_display::LayoutPosition::kBottom:
      return crosapi::mojom::DisplayLayoutPosition::kBottom;
    case system_display::LayoutPosition::kLeft:
    case system_display::LayoutPosition::kNone:
      return crosapi::mojom::DisplayLayoutPosition::kLeft;
  }
  NOTREACHED_IN_MIGRATION();
  return crosapi::mojom::DisplayLayoutPosition::kLeft;
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
      NOTREACHED_IN_MIGRATION();
      return crosapi::mojom::DisplayRotationOptions::kZeroDegrees;
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

  const display::Display& primary =
      display::Screen::GetScreen()->GetPrimaryDisplay();
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

system_display::DisplayMode GetDisplayModeFromMojo(
    const crosapi::mojom::DisplayMode mode) {
  system_display::DisplayMode result;
  result.width = mode.size.width();
  result.height = mode.size.height();
  result.width_in_native_pixels = mode.size_in_native_pixels.width();
  result.height_in_native_pixels = mode.size_in_native_pixels.height();
  result.device_scale_factor = mode.device_scale_factor;
  result.refresh_rate = mode.refresh_rate;
  result.is_native = mode.is_native;
  result.is_interlaced = mode.is_interlaced;
  return result;
}

system_display::DisplayUnitInfo GetDisplayUnitInfoFromMojo(
    const crosapi::mojom::DisplayUnitInfo& mojo_info) {
  system_display::DisplayUnitInfo info;
  info.id = mojo_info.id;
  info.name = mojo_info.name;
  if (mojo_info.edid) {
    info.edid.emplace();
    info.edid->manufacturer_id = mojo_info.edid->manufacturer_id;
    info.edid->product_id = mojo_info.edid->product_id;
    info.edid->year_of_manufacture = mojo_info.edid->year_of_manufacture;
  }
  info.is_primary = mojo_info.is_primary;
  info.is_internal = mojo_info.is_internal;
  info.active_state = mojo_info.is_detected
                          ? system_display::ActiveState::kActive
                          : system_display::ActiveState::kInactive;
  info.is_enabled = mojo_info.is_enabled;
  info.is_auto_rotation_allowed = mojo_info.is_auto_rotation_allowed;
  info.dpi_x = mojo_info.dpi_x;
  info.dpi_y = mojo_info.dpi_y;
  info.rotation =
      GetRotationFromMojomDisplayRotationInfo(mojo_info.rotation_options);
  const gfx::Rect& bounds = mojo_info.bounds;
  info.bounds.left = bounds.x();
  info.bounds.top = bounds.y();
  info.bounds.width = bounds.width();
  info.bounds.height = bounds.height();
  const gfx::Insets& overscan = mojo_info.overscan;
  info.overscan.left = overscan.left();
  info.overscan.top = overscan.top();
  info.overscan.right = overscan.right();
  info.overscan.bottom = overscan.bottom();
  const gfx::Rect& work_area = mojo_info.work_area;
  info.work_area.left = work_area.x();
  info.work_area.top = work_area.y();
  info.work_area.width = work_area.width();
  info.work_area.height = work_area.height();
  for (const crosapi::mojom::DisplayModePtr& mode :
       mojo_info.available_display_modes) {
    info.modes.emplace_back(GetDisplayModeFromMojo(*mode));
  }
  if (!info.modes.empty()) {
    int index = mojo_info.selected_display_mode_index;
    if (index < 0 || index >= static_cast<int>(info.modes.size())) {
      index = 0;
    }
    info.modes[index].is_selected = true;
  }
  info.has_touch_support = mojo_info.has_touch_support;
  info.has_accelerometer_support = mojo_info.has_accelerometer_support;
  info.available_display_zoom_factors =
      mojo_info.available_display_zoom_factors;
  info.display_zoom_factor = mojo_info.display_zoom_factor;
  return info;
}

crosapi::mojom::TouchCalibrationPairPtr GetTouchCalibrationPair(
    const system_display::TouchCalibrationPair& pair) {
  auto result = crosapi::mojom::TouchCalibrationPair::New();
  result->display_point =
      gfx::Point(pair.display_point.x, pair.display_point.y);
  result->touch_point = gfx::Point(pair.touch_point.x, pair.touch_point.y);
  return result;
}

void SetDisplayUnitInfoLayoutProperties(
    const crosapi::mojom::DisplayLayoutInfo& layout,
    system_display::DisplayUnitInfo* display) {
  display->is_unified =
      layout.layout_mode == crosapi::mojom::DisplayLayoutMode::kUnified;
  if (layout.mirror_source_id) {
    display->mirroring_source_id = *layout.mirror_source_id;
    if (layout.mirror_destination_ids) {
      for (const std::string& id : *layout.mirror_destination_ids) {
        display->mirroring_destination_ids.push_back(id);
      }
    }
  }
}

}  // namespace extensions
