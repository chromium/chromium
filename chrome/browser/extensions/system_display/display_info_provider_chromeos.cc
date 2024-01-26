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

void RunResultCallback(DisplayInfoProvider::ErrorCallback callback,
                       std::optional<std::string> error) {
  if (error) {
    LOG(ERROR) << "API call failed: " << *error;
  }
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(error)));
}

std::optional<std::string> GetStringResult(
    crosapi::mojom::DisplayConfigResult result) {
  switch (result) {
    case crosapi::mojom::DisplayConfigResult::kSuccess:
      return std::nullopt;
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
  std::optional<std::string> str_result = GetStringResult(result);
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
  std::optional<std::string> error =
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
    info.mode = system_display::MirrorMode::kNormal;
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
      id, crosapi::mojom::DisplayConfigOperation::kStart, std::nullopt,
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
      id, crosapi::mojom::DisplayConfigOperation::kReset, std::nullopt,
      base::BindOnce(&LogErrorResult));
  return true;
}

bool DisplayInfoProviderChromeOS::OverscanCalibrationComplete(
    const std::string& id) {
  cros_display_config_->OverscanCalibration(
      id, crosapi::mojom::DisplayConfigOperation::kComplete, std::nullopt,
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
                    ? std::nullopt
                    : GetStringResult(result));
          },
          std::move(callback)));
}

void DisplayInfoProviderChromeOS::SetMirrorMode(
    const api::system_display::MirrorModeInfo& info,
    ErrorCallback callback) {
  auto display_layout_info = crosapi::mojom::DisplayLayoutInfo::New();
  if (info.mode == api::system_display::MirrorMode::kOff) {
    display_layout_info->layout_mode =
        crosapi::mojom::DisplayLayoutMode::kNormal;
  } else {
    display_layout_info->layout_mode =
        crosapi::mojom::DisplayLayoutMode::kMirrored;
    if (info.mode == api::system_display::MirrorMode::kMixed) {
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
          std::make_optional<std::vector<std::string>>(
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
