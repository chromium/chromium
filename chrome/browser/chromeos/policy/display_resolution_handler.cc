// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/display_resolution_handler.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/optional.h"
#include "base/values.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chromeos/settings/cros_settings_names.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace policy {

using DisplayUnitTraits =
    mojo::StructTraits<::ash::mojom::DisplayUnitInfo::DataView,
                       ::ash::mojom::DisplayUnitInfoPtr>;

struct DisplayResolutionHandler::InternalDisplaySettings {
  int scale_percentage = 0;

  explicit InternalDisplaySettings(int scale_percentage)
      : scale_percentage(scale_percentage) {}

  bool operator==(const InternalDisplaySettings& rhs) const {
    return scale_percentage == rhs.scale_percentage;
  }

  bool operator!=(const InternalDisplaySettings& rhs) const {
    return !(*this == rhs);
  }

  // Create display config for the internal display using policy settings from
  // |internal_display_settings_|.
  ash::mojom::DisplayConfigPropertiesPtr ToDisplayConfigProperties() {
    auto new_config = ash::mojom::DisplayConfigProperties::New();
    // Converting percentage to factor.
    new_config->display_zoom_factor = scale_percentage / 100.0;
    return new_config;
  }

  // Get settings for the internal display from
  // |chromeos::kDeviceDisplayResolution| setting value.
  static std::unique_ptr<InternalDisplaySettings> FromPolicySetting(
      const base::DictionaryValue* pref) {
    const base::Value* scale_value =
        pref->FindKeyOfType(chromeos::kDeviceDisplayResolutionKeyInternalScale,
                            base::Value::Type::INTEGER);
    return scale_value ? std::make_unique<InternalDisplaySettings>(
                             scale_value->GetInt())
                       : nullptr;
  }
};

struct DisplayResolutionHandler::ExternalDisplaySettings {
  bool use_native = false;
  int width = 0;
  int height = 0;
  base::Optional<int> scale_percentage = base::nullopt;

  bool operator==(const ExternalDisplaySettings& rhs) const {
    return use_native == rhs.use_native && width == rhs.width &&
           height == rhs.height && scale_percentage == rhs.scale_percentage;
  }

  bool operator!=(const ExternalDisplaySettings& rhs) const {
    return !(*this == rhs);
  }

  // Check if either |use_native| flag is set and mode is native or the mode
  // has required resolution.
  bool IsSuitableDisplayMode(const ash::mojom::DisplayModePtr& mode) {
    return (use_native && mode->is_native) ||
           (!use_native && width == mode->size.width() &&
            height == mode->size.height());
  }

  // Create display config for the external display using policy settings from
  // |external_display_settings_|.
  ash::mojom::DisplayConfigPropertiesPtr ToDisplayConfigProperties(
      const std::vector<ash::mojom::DisplayModePtr>& display_modes) {
    bool found_suitable_mode = false;
    auto new_config = ash::mojom::DisplayConfigProperties::New();
    for (const ash::mojom::DisplayModePtr& mode : display_modes) {
      // Check if the current display mode has required resolution and its
      // refresh rate is higher than refresh rate of the already found mode.
      if (IsSuitableDisplayMode(mode) &&
          (!found_suitable_mode ||
           mode->refresh_rate > new_config->display_mode->refresh_rate)) {
        new_config->display_mode = mode->Clone();
        found_suitable_mode = true;
      }
    }
    // If we couldn't find the required mode and and scale percentage doesn't
    // need to be changed, we have nothing to do.
    if (!found_suitable_mode && !scale_percentage) {
      return ash::mojom::DisplayConfigPropertiesPtr();
    }

    if (scale_percentage) {
      // Converting percentage to the factor.
      new_config->display_zoom_factor = *scale_percentage / 100.0;
    }

    return new_config;
  }

  // Get settings for the external displays from
  // |chromeos::kDeviceDisplayResolution| setting value;
  static std::unique_ptr<ExternalDisplaySettings> FromPolicySetting(
      const base::DictionaryValue* pref) {
    const base::Value* width_value =
        pref->FindKeyOfType(chromeos::kDeviceDisplayResolutionKeyExternalWidth,
                            base::Value::Type::INTEGER);
    const base::Value* height_value =
        pref->FindKeyOfType(chromeos::kDeviceDisplayResolutionKeyExternalHeight,
                            base::Value::Type::INTEGER);
    const base::Value* scale_value =
        pref->FindKeyOfType(chromeos::kDeviceDisplayResolutionKeyExternalScale,
                            base::Value::Type::INTEGER);
    const base::Value* use_native_value = pref->FindKeyOfType(
        chromeos::kDeviceDisplayResolutionKeyExternalUseNative,
        base::Value::Type::BOOLEAN);

    auto result = std::make_unique<ExternalDisplaySettings>();

    // Scale can be used for both native and non-native modes
    if (scale_value)
      result->scale_percentage = scale_value->GetInt();

    if (use_native_value && use_native_value->GetBool()) {
      result->use_native = true;
      return result;
    }

    if (width_value && height_value) {
      result->width = width_value->GetInt();
      result->height = height_value->GetInt();
      return result;
    }

    return nullptr;
  }
};

DisplayResolutionHandler::DisplayResolutionHandler() = default;

DisplayResolutionHandler::~DisplayResolutionHandler() = default;

const char* DisplayResolutionHandler::SettingName() {
  return chromeos::kDeviceDisplayResolution;
}

// Reads |chromeos::kDeviceDisplayResolution| from CrosSettings and stores
// the settings in |recommended_|, |external_display_settings_| and
// |internal_display_settings_|. Also updates |policy_enabled_| flag.
void DisplayResolutionHandler::OnSettingUpdate() {
  policy_enabled_ = false;
  const base::DictionaryValue* resolution_pref = nullptr;
  chromeos::CrosSettings::Get()->GetDictionary(
      chromeos::kDeviceDisplayResolution, &resolution_pref);
  if (!resolution_pref)
    return;

  std::unique_ptr<ExternalDisplaySettings> new_external_config =
      ExternalDisplaySettings::FromPolicySetting(resolution_pref);
  std::unique_ptr<InternalDisplaySettings> new_internal_config =
      InternalDisplaySettings::FromPolicySetting(resolution_pref);

  bool new_recommended = false;
  policy_enabled_ = new_external_config || new_internal_config;
  const base::Value* recommended_value = resolution_pref->FindKeyOfType(
      chromeos::kDeviceDisplayResolutionKeyRecommended,
      base::Value::Type::BOOLEAN);

  if (recommended_value)
    new_recommended = recommended_value->GetBool();

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
    ash::mojom::CrosDisplayConfigController* cros_display_config,
    const std::vector<ash::mojom::DisplayUnitInfoPtr>& info_list) {
  if (!policy_enabled_)
    return;
  for (const ash::mojom::DisplayUnitInfoPtr& display_unit_info : info_list) {
    std::string display_id = display_unit_info->id;
    // If policy value is marked as "recommended" we need to change the
    // resolution just once for each display. So we're just skipping the display
    // if it was resized since last settings update.
    if (recommended_ &&
        resized_display_ids_.find(display_id) != resized_display_ids_.end()) {
      continue;
    }

    ash::mojom::DisplayConfigPropertiesPtr new_config;
    if (display_unit_info->is_internal && internal_display_settings_) {
      new_config = internal_display_settings_->ToDisplayConfigProperties();
    } else if (!display_unit_info->is_internal && external_display_settings_) {
      new_config = external_display_settings_->ToDisplayConfigProperties(
          DisplayUnitTraits::available_display_modes(display_unit_info));
    }

    if (!new_config)
      continue;

    resized_display_ids_.insert(display_id);
    cros_display_config->SetDisplayProperties(
        display_unit_info->id, std::move(new_config),
        ash::mojom::DisplayConfigSource::kPolicy,
        base::BindOnce([](ash::mojom::DisplayConfigResult result) {
          if (result == ash::mojom::DisplayConfigResult::kSuccess) {
            VLOG(1) << "Successfully changed display mode.";
          } else {
            LOG(ERROR) << "Couldn't change display mode. Error code: "
                       << result;
          }
        }));
  }
}

}  // namespace policy
