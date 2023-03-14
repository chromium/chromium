// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/system_display/display_info_provider_chromeos.h"
#include "base/task/single_thread_task_runner.h"

#include <stdint.h>
#include <cmath>

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/public/ash_interfaces.h"
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/lacros/lacros_service.h"
#endif

#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/extensions/system_display/display_info_provider.h"
#include "chrome/browser/extensions/system_display/display_info_provider_utils.h"
#include "extensions/common/api/system_display.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"

namespace extensions {

namespace system_display = api::system_display;

namespace {

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
    case system_display::LAYOUT_POSITION_TOP:
      return crosapi::mojom::DisplayLayoutPosition::kTop;
    case system_display::LAYOUT_POSITION_RIGHT:
      return crosapi::mojom::DisplayLayoutPosition::kRight;
    case system_display::LAYOUT_POSITION_BOTTOM:
      return crosapi::mojom::DisplayLayoutPosition::kBottom;
    case system_display::LAYOUT_POSITION_LEFT:
    case system_display::LAYOUT_POSITION_NONE:
      return crosapi::mojom::DisplayLayoutPosition::kLeft;
  }
  NOTREACHED();
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
      NOTREACHED();
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

// Validates the DisplayProperties input. Does not perform any tests with
// DisplayManager dependencies. Returns an error string on failure or nullopt
// on success.
absl::optional<std::string> ValidateDisplayPropertiesInput(
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
    return absl::nullopt;
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

  return absl::nullopt;
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

void RunResultCallback(DisplayInfoProvider::ErrorCallback callback,
                       absl::optional<std::string> error) {
  if (error) {
    LOG(ERROR) << "API call failed: " << *error;
  }
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(error)));
}

absl::optional<std::string> GetStringResult(
    crosapi::mojom::DisplayConfigResult result) {
  switch (result) {
    case crosapi::mojom::DisplayConfigResult::kSuccess:
      return absl::nullopt;
    case crosapi::mojom::DisplayConfigResult::kInvalidOperationError:
      return "Invalid operation";
    case crosapi::mojom::DisplayConfigResult::kInvalidDisplayIdError:
      return "Invalid display id";
    case crosapi::mojom::DisplayConfigResult::kUnifiedNotEnabledError:
      return "enableUnifiedDesktop must be called before setting is_unified";
    case crosapi::mojom::DisplayConfigResult::kPropertyValueOutOfRangeError:
      return "Property value out of range";
    case crosapi::mojom::DisplayConfigResult::
        kNotSupportedOnInternalDisplayError:
      return "Not supported for internal displays";
    case crosapi::mojom::DisplayConfigResult::kNegativeValueError:
      return "Negative values not supported";
    case crosapi::mojom::DisplayConfigResult::kSetDisplayModeError:
      return "Setting the display mode failed";
    case crosapi::mojom::DisplayConfigResult::kInvalidDisplayLayoutError:
      return "Invalid display layout";
    case crosapi::mojom::DisplayConfigResult::kSingleDisplayError:
      return "This mode requires multiple displays";
    case crosapi::mojom::DisplayConfigResult::kMirrorModeSourceIdError:
      return "Mirror mode source id invalid";
    case crosapi::mojom::DisplayConfigResult::kMirrorModeDestIdError:
      return "Mirror mode destination id invalid";
    case crosapi::mojom::DisplayConfigResult::kCalibrationNotAvailableError:
      return "Calibration not available";
    case crosapi::mojom::DisplayConfigResult::kCalibrationNotStartedError:
      return "Calibration not started";
    case crosapi::mojom::DisplayConfigResult::kCalibrationInProgressError:
      return "Calibration in progress";
    case crosapi::mojom::DisplayConfigResult::kCalibrationInvalidDataError:
      return "Calibration data invalid";
    case crosapi::mojom::DisplayConfigResult::kCalibrationFailedError:
      return "Calibration failed";
  }
  return "Unknown error";
}

void LogErrorResult(crosapi::mojom::DisplayConfigResult result) {
  absl::optional<std::string> str_result = GetStringResult(result);
  if (!str_result) {
    return;
  }
  LOG(ERROR) << *str_result;
}

}  // namespace

DisplayInfoProviderChromeOS::DisplayInfoProviderChromeOS(
    mojo::PendingRemote<crosapi::mojom::CrosDisplayConfigController>
        display_config)
    : cros_display_config_(std::move(display_config)) {}

DisplayInfoProviderChromeOS::~DisplayInfoProviderChromeOS() = default;

void DisplayInfoProviderChromeOS::SetDisplayProperties(
    const std::string& display_id_str,
    const api::system_display::DisplayProperties& properties,
    ErrorCallback callback) {
  absl::optional<std::string> error =
      ValidateDisplayPropertiesInput(display_id_str, properties);
  if (error) {
    RunResultCallback(std::move(callback), std::move(*error));
    return;
  }

  // Process the 'isUnified' property.
  if (properties.is_unified) {
    auto layout_info = crosapi::mojom::DisplayLayoutInfo::New();
    layout_info->layout_mode = *properties.is_unified
                                   ? crosapi::mojom::DisplayLayoutMode::kUnified
                                   : crosapi::mojom::DisplayLayoutMode::kNormal;
    cros_display_config_->SetDisplayLayoutInfo(
        std::move(layout_info),
        base::BindOnce(
            [](ErrorCallback callback,
               crosapi::mojom::DisplayConfigResult result) {
              std::move(callback).Run(GetStringResult(result));
            },
            std::move(callback)));
    // Note: If other properties are set they will be ignored.
    return;
  }

  // Process the deprecated 'mirroringSourceId' property. Validation ensures
  // that no other properties are set.
  if (properties.mirroring_source_id) {
    bool mirror = !properties.mirroring_source_id->empty();
    if (mirror) {
      // A display with the given id should exist and if should not be the same
      // as the target display's id.

      int64_t mirroring_id =
          GetDisplayForId(*properties.mirroring_source_id).id();
      if (mirroring_id == display::kInvalidDisplayId) {
        RunResultCallback(std::move(callback), "Invalid mirroring source id");
        return;
      }
      if (mirroring_id == GetDisplayId(display_id_str)) {
        RunResultCallback(std::move(callback), "Not allowed to mirror self");
        return;
      }
    }
    api::system_display::MirrorModeInfo info;
    info.mode = system_display::MIRROR_MODE_NORMAL;
    SetMirrorMode(info, std::move(callback));
    return;
  }

  // Global config properties.
  auto config_properties = crosapi::mojom::DisplayConfigProperties::New();
  config_properties->set_primary =
      properties.is_primary ? *properties.is_primary : false;
  if (properties.overscan) {
    config_properties->overscan = GetInsets(*properties.overscan);
  }
  if (properties.rotation) {
    config_properties->rotation = crosapi::mojom::DisplayRotation::New(
        GetMojomDisplayRotationOptions(*properties.rotation));
  }
  if (properties.bounds_origin_x || properties.bounds_origin_y) {
    gfx::Point bounds_origin;
    display::Display display = GetDisplayForId(display_id_str);
    if (display.id() != display::kInvalidDisplayId) {
      bounds_origin = display.bounds().origin();
    } else {
      DLOG(ERROR) << "Unable to get origin for display: " << display_id_str;
    }
    if (properties.bounds_origin_x) {
      bounds_origin.set_x(*properties.bounds_origin_x);
    }
    if (properties.bounds_origin_y) {
      bounds_origin.set_y(*properties.bounds_origin_y);
    }
    LOG(ERROR) << "Bounds origin: " << bounds_origin.ToString();
    config_properties->bounds_origin = std::move(bounds_origin);
  }
  config_properties->display_zoom_factor =
      properties.display_zoom_factor ? *properties.display_zoom_factor : 0;

  // Display mode.
  if (properties.display_mode) {
    auto mojo_display_mode = crosapi::mojom::DisplayMode::New();
    const api::system_display::DisplayMode& api_display_mode =
        *properties.display_mode;
    mojo_display_mode->size =
        gfx::Size(api_display_mode.width, api_display_mode.height);
    mojo_display_mode->size_in_native_pixels =
        gfx::Size(api_display_mode.width_in_native_pixels,
                  api_display_mode.height_in_native_pixels);
    mojo_display_mode->device_scale_factor =
        api_display_mode.device_scale_factor;
    mojo_display_mode->refresh_rate = api_display_mode.refresh_rate;
    mojo_display_mode->is_native = api_display_mode.is_native;
    mojo_display_mode->is_interlaced =
        api_display_mode.is_interlaced && *(api_display_mode.is_interlaced);
    config_properties->display_mode = std::move(mojo_display_mode);
  }

  cros_display_config_->SetDisplayProperties(
      display_id_str, std::move(config_properties),
      crosapi::mojom::DisplayConfigSource::kUser,
      base::BindOnce(
          [](ErrorCallback callback,
             crosapi::mojom::DisplayConfigResult result) {
            std::move(callback).Run(GetStringResult(result));
          },
          std::move(callback)));
}

void DisplayInfoProviderChromeOS::SetDisplayLayout(
    const DisplayLayoutList& layout_list,
    ErrorCallback callback) {
  auto layout_info = crosapi::mojom::DisplayLayoutInfo::New();
  // Generate the new list of layouts.
  std::vector<crosapi::mojom::DisplayLayoutPtr> display_layouts;
  for (const system_display::DisplayLayout& layout : layout_list) {
    auto display_layout = crosapi::mojom::DisplayLayout::New();
    display_layout->id = layout.id;
    display_layout->parent_id = layout.parent_id;
    display_layout->position = GetDisplayLayoutPosition(layout.position);
    display_layout->offset = layout.offset;
    display_layouts.emplace_back(std::move(display_layout));
  }
  layout_info->layouts = std::move(display_layouts);
  // We need to get the current layout info to provide the layout mode.
  cros_display_config_->GetDisplayLayoutInfo(
      base::BindOnce(&DisplayInfoProviderChromeOS::CallSetDisplayLayoutInfo,
                     weak_ptr_factory_.GetWeakPtr(), std::move(layout_info),
                     std::move(callback)));
}

void DisplayInfoProviderChromeOS::CallSetDisplayLayoutInfo(
    crosapi::mojom::DisplayLayoutInfoPtr layout_info,
    ErrorCallback callback,
    crosapi::mojom::DisplayLayoutInfoPtr cur_info) {
  // Copy the existing layout_mode.
  layout_info->layout_mode = cur_info->layout_mode;
  cros_display_config_->SetDisplayLayoutInfo(
      std::move(layout_info),
      base::BindOnce(
          [](ErrorCallback callback,
             crosapi::mojom::DisplayConfigResult result) {
            std::move(callback).Run(GetStringResult(result));
          },
          std::move(callback)));
}

void DisplayInfoProviderChromeOS::EnableUnifiedDesktop(bool enable) {
  cros_display_config_->SetUnifiedDesktopEnabled(enable);
}

void DisplayInfoProviderChromeOS::GetAllDisplaysInfo(
    bool single_unified,
    base::OnceCallback<void(DisplayUnitInfoList result)> callback) {
  cros_display_config_->GetDisplayLayoutInfo(base::BindOnce(
      &DisplayInfoProviderChromeOS::CallGetDisplayUnitInfoList,
      weak_ptr_factory_.GetWeakPtr(), single_unified, std::move(callback)));
}

void DisplayInfoProviderChromeOS::CallGetDisplayUnitInfoList(
    bool single_unified,
    base::OnceCallback<void(DisplayUnitInfoList result)> callback,
    crosapi::mojom::DisplayLayoutInfoPtr layout) {
  cros_display_config_->GetDisplayUnitInfoList(
      single_unified,
      base::BindOnce(&DisplayInfoProviderChromeOS::OnGetDisplayUnitInfoList,
                     weak_ptr_factory_.GetWeakPtr(), std::move(layout),
                     std::move(callback)));
}

void DisplayInfoProviderChromeOS::OnGetDisplayUnitInfoList(
    crosapi::mojom::DisplayLayoutInfoPtr layout,
    base::OnceCallback<void(DisplayUnitInfoList)> callback,
    std::vector<crosapi::mojom::DisplayUnitInfoPtr> info_list) {
  DisplayUnitInfoList all_displays;
  for (const crosapi::mojom::DisplayUnitInfoPtr& info : info_list) {
    system_display::DisplayUnitInfo display = GetDisplayUnitInfoFromMojo(*info);
    SetDisplayUnitInfoLayoutProperties(*layout, &display);
    all_displays.push_back(std::move(display));
  }
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(all_displays)));
}

void DisplayInfoProviderChromeOS::GetDisplayLayout(
    base::OnceCallback<void(DisplayLayoutList)> callback) {
  cros_display_config_->GetDisplayLayoutInfo(
      base::BindOnce(&OnGetDisplayLayoutResult, std::move(callback)));
}

bool DisplayInfoProviderChromeOS::OverscanCalibrationStart(
    const std::string& id) {
  cros_display_config_->OverscanCalibration(
      id, crosapi::mojom::DisplayConfigOperation::kStart, absl::nullopt,
      base::BindOnce(&LogErrorResult));
  return true;
}

bool DisplayInfoProviderChromeOS::OverscanCalibrationAdjust(
    const std::string& id,
    const system_display::Insets& delta) {
  cros_display_config_->OverscanCalibration(
      id, crosapi::mojom::DisplayConfigOperation::kAdjust, GetInsets(delta),
      base::BindOnce(&LogErrorResult));
  return true;
}

bool DisplayInfoProviderChromeOS::OverscanCalibrationReset(
    const std::string& id) {
  cros_display_config_->OverscanCalibration(
      id, crosapi::mojom::DisplayConfigOperation::kReset, absl::nullopt,
      base::BindOnce(&LogErrorResult));
  return true;
}

bool DisplayInfoProviderChromeOS::OverscanCalibrationComplete(
    const std::string& id) {
  cros_display_config_->OverscanCalibration(
      id, crosapi::mojom::DisplayConfigOperation::kComplete, absl::nullopt,
      base::BindOnce(&LogErrorResult));
  return true;
}

void DisplayInfoProviderChromeOS::ShowNativeTouchCalibration(
    const std::string& id,
    ErrorCallback callback) {
  CallTouchCalibration(id, crosapi::mojom::DisplayConfigOperation::kShowNative,
                       nullptr, std::move(callback));
}

bool DisplayInfoProviderChromeOS::StartCustomTouchCalibration(
    const std::string& id) {
  touch_calibration_target_id_ = id;
  CallTouchCalibration(id, crosapi::mojom::DisplayConfigOperation::kStart,
                       nullptr, ErrorCallback());
  return true;
}

bool DisplayInfoProviderChromeOS::CompleteCustomTouchCalibration(
    const api::system_display::TouchCalibrationPairQuad& pairs,
    const api::system_display::Bounds& bounds) {
  auto calibration = crosapi::mojom::TouchCalibration::New();
  calibration->pairs.emplace_back(GetTouchCalibrationPair(pairs.pair1));
  calibration->pairs.emplace_back(GetTouchCalibrationPair(pairs.pair2));
  calibration->pairs.emplace_back(GetTouchCalibrationPair(pairs.pair3));
  calibration->pairs.emplace_back(GetTouchCalibrationPair(pairs.pair4));
  calibration->bounds = gfx::Size(bounds.width, bounds.height);
  CallTouchCalibration(touch_calibration_target_id_,
                       crosapi::mojom::DisplayConfigOperation::kComplete,
                       std::move(calibration), ErrorCallback());
  return true;
}

bool DisplayInfoProviderChromeOS::ClearTouchCalibration(const std::string& id) {
  CallTouchCalibration(id, crosapi::mojom::DisplayConfigOperation::kReset,
                       nullptr, ErrorCallback());
  return true;
}

void DisplayInfoProviderChromeOS::CallTouchCalibration(
    const std::string& id,
    crosapi::mojom::DisplayConfigOperation op,
    crosapi::mojom::TouchCalibrationPtr calibration,
    ErrorCallback callback) {
  cros_display_config_->TouchCalibration(
      id, op, std::move(calibration),
      base::BindOnce(
          [](ErrorCallback callback,
             crosapi::mojom::DisplayConfigResult result) {
            if (!callback) {
              return;
            }
            std::move(callback).Run(
                result == crosapi::mojom::DisplayConfigResult::kSuccess
                    ? absl::nullopt
                    : GetStringResult(result));
          },
          std::move(callback)));
}

void DisplayInfoProviderChromeOS::SetMirrorMode(
    const api::system_display::MirrorModeInfo& info,
    ErrorCallback callback) {
  auto display_layout_info = crosapi::mojom::DisplayLayoutInfo::New();
  if (info.mode == api::system_display::MIRROR_MODE_OFF) {
    display_layout_info->layout_mode =
        crosapi::mojom::DisplayLayoutMode::kNormal;
  } else {
    display_layout_info->layout_mode =
        crosapi::mojom::DisplayLayoutMode::kMirrored;
    if (info.mode == api::system_display::MIRROR_MODE_MIXED) {
      if (!info.mirroring_source_id) {
        RunResultCallback(std::move(callback), "Mirror mode source id invalid");
        return;
      }
      if (!info.mirroring_destination_ids) {
        RunResultCallback(std::move(callback),
                          "Mixed mirror mode requires destination ids");
        return;
      }
      display_layout_info->mirror_source_id = *info.mirroring_source_id;
      display_layout_info->mirror_destination_ids =
          absl::make_optional<std::vector<std::string>>(
              *info.mirroring_destination_ids);
    }
  }
  cros_display_config_->SetDisplayLayoutInfo(
      std::move(display_layout_info),
      base::BindOnce(
          [](ErrorCallback callback,
             crosapi::mojom::DisplayConfigResult result) {
            std::move(callback).Run(GetStringResult(result));
          },
          std::move(callback)));
}

void DisplayInfoProviderChromeOS::StartObserving() {
  DisplayInfoProvider::StartObserving();

  mojo::PendingAssociatedRemote<crosapi::mojom::CrosDisplayConfigObserver>
      observer;
  cros_display_config_observer_receiver_.Bind(
      observer.InitWithNewEndpointAndPassReceiver());
  cros_display_config_->AddObserver(std::move(observer));
}

void DisplayInfoProviderChromeOS::StopObserving() {
  DisplayInfoProvider::StopObserving();

  cros_display_config_observer_receiver_.reset();
}

void DisplayInfoProviderChromeOS::OnDisplayConfigChanged() {
  DispatchOnDisplayChangedEvent();
}

#if BUILDFLAG(IS_CHROMEOS_ASH)

std::unique_ptr<DisplayInfoProvider> CreateChromeDisplayInfoProvider() {
  mojo::PendingRemote<crosapi::mojom::CrosDisplayConfigController>
      display_config;
  ash::BindCrosDisplayConfigController(
      display_config.InitWithNewPipeAndPassReceiver());
  return std::make_unique<DisplayInfoProviderChromeOS>(
      std::move(display_config));
}

#elif BUILDFLAG(IS_CHROMEOS_LACROS)

std::unique_ptr<DisplayInfoProvider> CreateChromeDisplayInfoProvider() {
  // Assume that LacrosService has already been initialized.
  auto* lacros_service = chromeos::LacrosService::Get();
  if (lacros_service &&
      lacros_service
          ->IsAvailable<crosapi::mojom::CrosDisplayConfigController>()) {
    auto& remote =
        lacros_service
            ->GetRemote<crosapi::mojom::CrosDisplayConfigController>();
    return std::make_unique<DisplayInfoProviderChromeOS>(remote.Unbind());
  }

  LOG(ERROR) << "Cannot create a DisplayInfoProvider instance in Lacros. "
                "CrosDisplayConfigController interface is not available.";
  return nullptr;
}

#endif

}  // namespace extensions
