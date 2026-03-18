// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/system_display/display_info_provider_chromeos.h"

#include <stdint.h>

#include <cmath>

#include "ash/display/cros_display_config.h"
#include "ash/shell.h"
#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "base/types/optional_ref.h"
#include "chrome/browser/extensions/system_display/display_info_provider.h"
#include "chrome/browser/extensions/system_display/display_info_provider_utils.h"
#include "extensions/common/api/system_display.h"
#include "ui/display/display.h"
#include "ui/display/display_layout.h"
#include "ui/display/manager/touch_device_manager.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"

namespace extensions {

namespace system_display = api::system_display;

namespace {

void RunResultCallback(DisplayInfoProvider::ErrorCallback callback,
                       std::optional<std::string> error) {
  if (error) {
    LOG(ERROR) << "API call failed: " << *error;
  }
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(error)));
}

std::optional<std::string> GetStringResult(ash::DisplayConfigResult result) {
  switch (result) {
    case ash::DisplayConfigResult::kSuccess:
      return std::nullopt;
    case ash::DisplayConfigResult::kInvalidOperationError:
      return "Invalid operation";
    case ash::DisplayConfigResult::kInvalidDisplayIdError:
      return "Invalid display id";
    case ash::DisplayConfigResult::kUnifiedNotEnabledError:
      return "enableUnifiedDesktop must be called before setting is_unified";
    case ash::DisplayConfigResult::kPropertyValueOutOfRangeError:
      return "Property value out of range";
    case ash::DisplayConfigResult::kNotSupportedOnInternalDisplayError:
      return "Not supported for internal displays";
    case ash::DisplayConfigResult::kNegativeValueError:
      return "Negative values not supported";
    case ash::DisplayConfigResult::kSetDisplayModeError:
      return "Setting the display mode failed";
    case ash::DisplayConfigResult::kInvalidDisplayLayoutError:
      return "Invalid display layout";
    case ash::DisplayConfigResult::kSingleDisplayError:
      return "This mode requires multiple displays";
    case ash::DisplayConfigResult::kMirrorModeSourceIdError:
      return "Mirror mode source id invalid";
    case ash::DisplayConfigResult::kMirrorModeDestIdError:
      return "Mirror mode destination id invalid";
    case ash::DisplayConfigResult::kCalibrationNotAvailableError:
      return "Calibration not available";
    case ash::DisplayConfigResult::kCalibrationNotStartedError:
      return "Calibration not started";
    case ash::DisplayConfigResult::kCalibrationInProgressError:
      return "Calibration in progress";
    case ash::DisplayConfigResult::kCalibrationInvalidDataError:
      return "Calibration data invalid";
    case ash::DisplayConfigResult::kCalibrationFailedError:
      return "Calibration failed";
  }
  return "Unknown error";
}

void LogErrorResult(ash::DisplayConfigResult result) {
  std::optional<std::string> str_result = GetStringResult(result);
  if (str_result) {
    LOG(ERROR) << *str_result;
  }
}

system_display::LayoutPosition GetLayoutPositionFromUi(
    display::DisplayPlacement::Position position) {
  switch (position) {
    case display::DisplayPlacement::TOP:
      return system_display::LayoutPosition::kTop;
    case display::DisplayPlacement::RIGHT:
      return system_display::LayoutPosition::kRight;
    case display::DisplayPlacement::BOTTOM:
      return system_display::LayoutPosition::kBottom;
    case display::DisplayPlacement::LEFT:
      return system_display::LayoutPosition::kLeft;
  }
  NOTREACHED();
}

}  // namespace

DisplayInfoProviderChromeOS::DisplayInfoProviderChromeOS()
    : cros_display_config_(ash::Shell::Get()->cros_display_config()) {
  CHECK(cros_display_config_);
  shell_observation_.Observe(ash::Shell::Get());
}

DisplayInfoProviderChromeOS::~DisplayInfoProviderChromeOS() = default;

void DisplayInfoProviderChromeOS::OnShellDestroying() {
  shell_observation_.Reset();
  cros_display_config_observation_.Reset();
  cros_display_config_ = nullptr;
}

void DisplayInfoProviderChromeOS::SetDisplayProperties(
    const std::string& display_id_str,
    const api::system_display::DisplayProperties& properties,
    ErrorCallback callback) {
  std::optional<std::string> error =
      ValidateDisplayPropertiesInput(display_id_str, properties);
  if (error) {
    RunResultCallback(std::move(callback), std::move(*error));
    return;
  }

  // Process the 'isUnified' property.
  if (properties.is_unified && cros_display_config_) {
    ash::DisplayLayoutInfo layout_info;
    layout_info.layout_mode = *properties.is_unified
                                  ? ash::DisplayLayoutMode::kUnified
                                  : ash::DisplayLayoutMode::kNormal;
    ash::DisplayConfigResult result =
        cros_display_config_->SetDisplayLayoutInfo(std::move(layout_info));
    std::move(callback).Run(GetStringResult(result));
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
    info.mode = system_display::MirrorMode::kNormal;
    SetMirrorMode(info, std::move(callback));
    return;
  }

  // Global config properties.
  ash::DisplayConfigProperties config_properties;
  config_properties.set_primary =
      properties.is_primary ? *properties.is_primary : false;
  if (properties.overscan.has_value()) {
    config_properties.overscan = GetInsets(*properties.overscan);
  }
  if (properties.rotation.has_value()) {
    config_properties.rotation =
        GetMojomDisplayRotationOptions(*properties.rotation);
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
    config_properties.bounds_origin = std::move(bounds_origin);
  }
  config_properties.display_zoom_factor =
      properties.display_zoom_factor.has_value()
          ? *properties.display_zoom_factor
          : 0;

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
    config_properties.display_mode = std::move(mojo_display_mode);
  }

  if (cros_display_config_) {
    ash::DisplayConfigResult result =
        cros_display_config_->SetDisplayProperties(
            display_id_str, std::move(config_properties),
            crosapi::mojom::DisplayConfigSource::kUser);
    std::move(callback).Run(GetStringResult(std::move(result)));
  }
}

base::expected<void, std::string> DisplayInfoProviderChromeOS::SetDisplayLayout(
    const DisplayLayoutList& layout_list) {
  CHECK(cros_display_config_);

  // Generate the new list of layouts.
  std::vector<display::DisplayPlacement> display_layouts;
  for (const system_display::DisplayLayout& layout : layout_list) {
    display::DisplayPlacement display_layout;
    display_layout.display_id = GetDisplayId(layout.id);
    display_layout.parent_display_id = GetDisplayId(layout.parent_id);
    display_layout.position = GetDisplayLayoutPosition(layout.position);
    display_layout.offset = layout.offset;
    display_layouts.emplace_back(std::move(display_layout));
  }

  ash::DisplayLayoutInfo layout_info;
  layout_info.layouts = std::move(display_layouts);

  // We need to get the current layout info to provide the layout mode.
  ash::DisplayLayoutInfo cur_info =
      cros_display_config_->GetDisplayLayoutInfo();

  // Copy the existing layout_mode.
  layout_info.layout_mode = cur_info.layout_mode;
  ash::DisplayConfigResult result =
      cros_display_config_->SetDisplayLayoutInfo(std::move(layout_info));
  if (auto r = GetStringResult(result); r.has_value()) {
    return base::unexpected(*r);
  }
  return base::ok();
}

void DisplayInfoProviderChromeOS::EnableUnifiedDesktop(bool enable) {
  if (cros_display_config_) {
    cros_display_config_->SetUnifiedDesktopEnabled(enable);
  }
}

void DisplayInfoProviderChromeOS::GetAllDisplaysInfo(
    bool single_unified,
    base::OnceCallback<void(DisplayUnitInfoList result)> callback) {
  if (cros_display_config_) {
    ash::DisplayLayoutInfo layout =
        cros_display_config_->GetDisplayLayoutInfo();
    std::vector<crosapi::mojom::DisplayUnitInfoPtr> info_list =
        cros_display_config_->GetDisplayUnitInfoList(single_unified);
    DisplayUnitInfoList all_displays;
    for (const crosapi::mojom::DisplayUnitInfoPtr& info : info_list) {
      system_display::DisplayUnitInfo display =
          GetDisplayUnitInfoFromMojo(*info);
      SetDisplayUnitInfoLayoutProperties(layout, &display);
      all_displays.push_back(std::move(display));
    }
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), std::move(all_displays)));
  }
}

DisplayInfoProvider::DisplayLayoutList
DisplayInfoProviderChromeOS::GetDisplayLayout() {
  CHECK(cros_display_config_);
  DisplayInfoProvider::DisplayLayoutList result;
  ash::DisplayLayoutInfo info = cros_display_config_->GetDisplayLayoutInfo();
  if (info.layouts.has_value()) {
    for (const display::DisplayPlacement& layout : *info.layouts) {
      api::system_display::DisplayLayout display_layout;
      display_layout.id = base::NumberToString(layout.display_id);
      display_layout.parent_id = base::NumberToString(layout.parent_display_id);
      display_layout.position = GetLayoutPositionFromUi(layout.position);
      display_layout.offset = layout.offset;
      result.emplace_back(std::move(display_layout));
    }
  }
  return result;
}

bool DisplayInfoProviderChromeOS::OverscanCalibrationStart(
    const std::string& id) {
  if (cros_display_config_) {
    ash::DisplayConfigResult result = cros_display_config_->OverscanCalibration(
        id, crosapi::mojom::DisplayConfigOperation::kStart, std::nullopt);
    LogErrorResult(result);
  }
  return true;
}

bool DisplayInfoProviderChromeOS::OverscanCalibrationAdjust(
    const std::string& id,
    const system_display::Insets& delta) {
  if (cros_display_config_) {
    ash::DisplayConfigResult result = cros_display_config_->OverscanCalibration(
        id, crosapi::mojom::DisplayConfigOperation::kAdjust, GetInsets(delta));
    LogErrorResult(result);
  }
  return true;
}

bool DisplayInfoProviderChromeOS::OverscanCalibrationReset(
    const std::string& id) {
  if (cros_display_config_) {
    ash::DisplayConfigResult result = cros_display_config_->OverscanCalibration(
        id, crosapi::mojom::DisplayConfigOperation::kReset, std::nullopt);
    LogErrorResult(result);
  }
  return true;
}

bool DisplayInfoProviderChromeOS::OverscanCalibrationComplete(
    const std::string& id) {
  if (cros_display_config_) {
    ash::DisplayConfigResult result = cros_display_config_->OverscanCalibration(
        id, crosapi::mojom::DisplayConfigOperation::kComplete, std::nullopt);
    LogErrorResult(result);
  }
  return true;
}

void DisplayInfoProviderChromeOS::ShowNativeTouchCalibration(
    const std::string& id,
    ErrorCallback callback) {
  CallTouchCalibration(id, crosapi::mojom::DisplayConfigOperation::kShowNative,
                       std::nullopt, std::move(callback));
}

bool DisplayInfoProviderChromeOS::StartCustomTouchCalibration(
    const std::string& id) {
  touch_calibration_target_id_ = id;
  CallTouchCalibration(id, crosapi::mojom::DisplayConfigOperation::kStart,
                       std::nullopt, ErrorCallback());
  return true;
}

bool DisplayInfoProviderChromeOS::CompleteCustomTouchCalibration(
    const api::system_display::TouchCalibrationPairQuad& pairs,
    const api::system_display::Bounds& bounds) {
  display::TouchCalibrationData calibration;
  calibration.point_pairs[0] = GetTouchCalibrationPair(pairs.pair1);
  calibration.point_pairs[1] = GetTouchCalibrationPair(pairs.pair2);
  calibration.point_pairs[2] = GetTouchCalibrationPair(pairs.pair3);
  calibration.point_pairs[3] = GetTouchCalibrationPair(pairs.pair4);
  calibration.bounds = gfx::Size(bounds.width, bounds.height);
  CallTouchCalibration(touch_calibration_target_id_,
                       crosapi::mojom::DisplayConfigOperation::kComplete,
                       calibration, ErrorCallback());
  return true;
}

bool DisplayInfoProviderChromeOS::ClearTouchCalibration(const std::string& id) {
  CallTouchCalibration(id, crosapi::mojom::DisplayConfigOperation::kReset,
                       std::nullopt, ErrorCallback());
  return true;
}

void DisplayInfoProviderChromeOS::CallTouchCalibration(
    const std::string& id,
    crosapi::mojom::DisplayConfigOperation op,
    base::optional_ref<const display::TouchCalibrationData> calibration,
    ErrorCallback callback) {
  if (cros_display_config_) {
    cros_display_config_->TouchCalibration(
        id, op, calibration,
        base::BindOnce(
            [](ErrorCallback callback, ash::DisplayConfigResult result) {
              if (!callback) {
                return;
              }
              std::move(callback).Run(result ==
                                              ash::DisplayConfigResult::kSuccess
                                          ? std::nullopt
                                          : GetStringResult(result));
            },
            std::move(callback)));
  }
}

void DisplayInfoProviderChromeOS::SetMirrorMode(
    const api::system_display::MirrorModeInfo& info,
    ErrorCallback callback) {
  ash::DisplayLayoutInfo display_layout_info;
  if (info.mode == api::system_display::MirrorMode::kOff) {
    display_layout_info.layout_mode = ash::DisplayLayoutMode::kNormal;
  } else {
    display_layout_info.layout_mode = ash::DisplayLayoutMode::kMirrored;
    if (info.mode == api::system_display::MirrorMode::kMixed) {
      if (!info.mirroring_source_id) {
        RunResultCallback(std::move(callback), "Mirror mode source id invalid");
        return;
      }
      if (!info.mirroring_destination_ids.has_value()) {
        RunResultCallback(std::move(callback),
                          "Mixed mirror mode requires destination ids");
        return;
      }
      display_layout_info.mirror_source_id =
          GetDisplayId(*info.mirroring_source_id);
      display_layout_info.mirror_destination_ids.emplace();
      for (const auto& id_str : *info.mirroring_destination_ids) {
        display_layout_info.mirror_destination_ids->push_back(
            GetDisplayId(id_str));
      }
    }
  }
  if (cros_display_config_) {
    ash::DisplayConfigResult result =
        cros_display_config_->SetDisplayLayoutInfo(
            std::move(display_layout_info));
    std::move(callback).Run(GetStringResult(result));
  }
}

void DisplayInfoProviderChromeOS::StartObserving() {
  DisplayInfoProvider::StartObserving();
  if (cros_display_config_) {
    cros_display_config_observation_.Observe(cros_display_config_);
  }
}

void DisplayInfoProviderChromeOS::StopObserving() {
  cros_display_config_observation_.Reset();
  DisplayInfoProvider::StopObserving();
}

void DisplayInfoProviderChromeOS::OnDisplayConfigChanged() {
  DispatchOnDisplayChangedEvent();
}

std::unique_ptr<DisplayInfoProvider> CreateChromeDisplayInfoProvider() {
  return std::make_unique<DisplayInfoProviderChromeOS>();
}

}  // namespace extensions
