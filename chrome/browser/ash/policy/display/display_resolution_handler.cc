// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/display/display_resolution_handler.h"

#include <optional>
#include <utility>
#include <vector>

#include "ash/display/cros_display_config.h"
#include "ash/shell.h"
#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace policy {

struct DisplayResolutionHandler::InternalDisplaySettings {
  int scale_percentage = 0;

  explicit InternalDisplaySettings(int scale_percentage)
      : scale_percentage(scale_percentage) {}

  friend bool operator==(const InternalDisplaySettings&,
                         const InternalDisplaySettings&) = default;

  // Create display config for the internal display using policy settings from
  // |internal_display_settings_|.
  std::optional<ash::DisplayConfigProperties> ToDisplayConfigProperties() {
    ash::DisplayConfigProperties new_config;
    // Converting percentage to factor.
    new_config.display_zoom_factor = scale_percentage / 100.0;
    return new_config;
  }

  // Get settings for the internal display from
  // |ash::kDeviceDisplayResolution| setting value.
  static std::unique_ptr<InternalDisplaySettings> FromPolicySetting(
      const base::DictValue* pref) {
    const std::optional<int> scale_value =
        pref->FindInt(ash::kDeviceDisplayResolutionKeyInternalScale);
    return scale_value ? std::make_unique<InternalDisplaySettings>(*scale_value)
                       : nullptr;
  }
};

struct DisplayResolutionHandler::ExternalDisplaySettings {
  bool use_native = false;
  int width = 0;
  int height = 0;
  std::optional<int> scale_percentage = std::nullopt;

  bool operator==(const ExternalDisplaySettings& rhs) const {
    return use_native == rhs.use_native && width == rhs.width &&
           height == rhs.height && scale_percentage == rhs.scale_percentage;
  }

  bool operator!=(const ExternalDisplaySettings& rhs) const {
    return !(*this == rhs);
  }

  // Check if either |use_native| flag is set and mode is native or the mode
  // has required resolution.
  bool IsSuitableDisplayMode(const display::ManagedDisplayMode& mode) {
    return use_native ? mode.native()
                      : (gfx::Size(width, height) == mode.GetSizeInDIP());
  }

  // Create display config for the external display using policy settings from
  // |external_display_settings_|.
  std::optional<ash::DisplayConfigProperties> ToDisplayConfigProperties(
      const std::vector<display::ManagedDisplayMode>& display_modes) {
    ash::DisplayConfigProperties new_config;
    CHECK(!new_config.display_mode.has_value());
    for (const auto& mode : display_modes) {
      // Check if `mode` has the required resolution and its refresh rate is
      // higher than refresh rate of the already found mode.
      if (IsSuitableDisplayMode(mode) &&
          (!new_config.display_mode.has_value() ||
           mode.refresh_rate() > new_config.display_mode->refresh_rate())) {
        new_config.display_mode = mode;
      }
    }
    // If we couldn't find the required mode and and scale percentage doesn't
    // need to be changed, we have nothing to do.
    if (!new_config.display_mode.has_value() && !scale_percentage) {
      return std::nullopt;
    }

    if (scale_percentage) {
      // Converting percentage to the factor.
      new_config.display_zoom_factor = *scale_percentage / 100.0;
    }

    return new_config;
  }

  // Get settings for the external displays from
  // |ash::kDeviceDisplayResolution| setting value;
  static std::unique_ptr<ExternalDisplaySettings> FromPolicySetting(
      const base::DictValue* pref) {
    auto result = std::make_unique<ExternalDisplaySettings>();

    // Scale can be used for both native and non-native modes
    result->scale_percentage =
        pref->FindInt(ash::kDeviceDisplayResolutionKeyExternalScale);

    const std::optional<bool> use_native_value =
        pref->FindBool(ash::kDeviceDisplayResolutionKeyExternalUseNative);
    if (use_native_value && *use_native_value) {
      result->use_native = true;
      return result;
    }

    const std::optional<int> width_value =
        pref->FindInt(ash::kDeviceDisplayResolutionKeyExternalWidth);
    const std::optional<int> height_value =
        pref->FindInt(ash::kDeviceDisplayResolutionKeyExternalHeight);
    if (width_value && height_value) {
      result->width = *width_value;
      result->height = *height_value;
      return result;
    }

    return nullptr;
  }
};

DisplayResolutionHandler::DisplayResolutionHandler() = default;

DisplayResolutionHandler::~DisplayResolutionHandler() = default;

const char* DisplayResolutionHandler::SettingName() {
  return ash::kDeviceDisplayResolution;
}

// Reads |ash::kDeviceDisplayResolution| from CrosSettings and stores
// the settings in |recommended_|, |external_display_settings_| and
// |internal_display_settings_|. Also updates |policy_enabled_| flag.
void DisplayResolutionHandler::OnSettingUpdate() {
  policy_enabled_ = false;
  const base::DictValue* resolution_pref = nullptr;
  ash::CrosSettings::Get()->GetDictionary(ash::kDeviceDisplayResolution,
                                          &resolution_pref);
  if (!resolution_pref)
    return;

  std::unique_ptr<ExternalDisplaySettings> new_external_config =
      ExternalDisplaySettings::FromPolicySetting(resolution_pref);
  std::unique_ptr<InternalDisplaySettings> new_internal_config =
      InternalDisplaySettings::FromPolicySetting(resolution_pref);

  bool new_recommended = false;
  policy_enabled_ = new_external_config || new_internal_config;
  const std::optional<bool> recommended_value =
      resolution_pref->FindBool(ash::kDeviceDisplayResolutionKeyRecommended);

  if (recommended_value)
    new_recommended = *recommended_value;

  // We should reset locally stored settings and clear list of already updated
  // displays if any of the policy values were updated.
  bool should_reset_settings = false;
  should_reset_settings |=
      bool{new_external_config} != bool{external_display_settings_};
  should_reset_settings |= new_external_config && external_display_settings_ &&
                           *new_external_config != *external_display_settings_;
  should_reset_settings |=
      bool{new_internal_config} != bool{internal_display_settings_};
  should_reset_settings |= new_internal_config && internal_display_settings_ &&
                           *new_internal_config != *internal_display_settings_;
  should_reset_settings |= recommended_ != new_recommended;

  if (!should_reset_settings)
    return;

  resized_display_ids_.clear();
  external_display_settings_ = std::move(new_external_config);
  internal_display_settings_ = std::move(new_internal_config);
  recommended_ = new_recommended;
}

// Applies settings received with |OnSettingUpdate| to each supported display
// from |info_list| if |policy_enabled_| is true.
void DisplayResolutionHandler::ApplyChanges(
    ash::CrosDisplayConfig& cros_display_config,
    const std::vector<ash::DisplayUnitInfo>& info_list) {
  if (!policy_enabled_)
    return;
  for (const auto& display_unit_info : info_list) {
    // If policy value is marked as "recommended" we need to change the
    // resolution just once for each display. So we're just skipping the display
    // if it was resized since last settings update.
    if (recommended_ && resized_display_ids_.find(display_unit_info.id) !=
                            resized_display_ids_.end()) {
      continue;
    }

    std::optional<ash::DisplayConfigProperties> new_config;
    if (display_unit_info.is_internal && internal_display_settings_) {
      new_config = internal_display_settings_->ToDisplayConfigProperties();
    } else if (!display_unit_info.is_internal && external_display_settings_) {
      new_config = external_display_settings_->ToDisplayConfigProperties(
          display_unit_info.available_display_modes);
    }
    if (!new_config.has_value()) {
      continue;
    }

    resized_display_ids_.insert(display_unit_info.id);
    ash::DisplayConfigResult result = cros_display_config.SetDisplayProperties(
        base::NumberToString(display_unit_info.id), *new_config,
        crosapi::mojom::DisplayConfigSource::kPolicy);
    if (result == ash::DisplayConfigResult::kSuccess) {
      VLOG(1) << "Successfully changed display mode.";
    } else {
      LOG(ERROR) << "Couldn't change display mode. Error code: "
                 << static_cast<int>(result);
    }
  }
}

}  // namespace policy
