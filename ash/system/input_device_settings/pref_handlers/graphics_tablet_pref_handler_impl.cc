// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/pref_handlers/graphics_tablet_pref_handler_impl.h"

#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/public/mojom/input_device_settings.mojom.h"
#include "ash/system/input_device_settings/input_device_settings_metadata.h"
#include "ash/system/input_device_settings/input_device_settings_pref_names.h"
#include "ash/system/input_device_settings/input_device_settings_utils.h"
#include "base/json/values_util.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/known_user.h"

namespace ash {

namespace {

// Graphics tablet metadata was previously added incorrectly for a set of
// graphics tablets. This trims out the added buttons that do not exist that
// were previously added via metadata.
// TODO(dpad): Remove after 07/2025 (M139) as this trimming will no longer be
// needed.
void TrimButtonRemappingListForGraphicsTabletPen(
    mojom::GraphicsTablet* graphics_tablet) {
  CHECK(graphics_tablet);
  if (graphics_tablet->graphics_tablet_button_config ==
      mojom::GraphicsTabletButtonConfig::kNoConfig) {
    return;
  }

  auto metadata_pen_button_list = GetPenButtonRemappingListForConfig(
      graphics_tablet->graphics_tablet_button_config);
  if (metadata_pen_button_list.size() ==
      graphics_tablet->settings->pen_button_remappings.size()) {
    return;
  }

  // For each pen button, if it matches a saved button, copy over the remapping
  // action. The `metadata_pen_button_list` will be the final source of truth
  // after this trimming happens.
  for (auto& metadata_button_remapping : metadata_pen_button_list) {
    for (const auto& saved_button_remapping :
         graphics_tablet->settings->pen_button_remappings) {
      if (metadata_button_remapping->button == saved_button_remapping->button) {
        metadata_button_remapping->remapping_action =
            saved_button_remapping->remapping_action
                ? saved_button_remapping->remapping_action->Clone()
                : nullptr;
      }
    }
  }

  graphics_tablet->settings->pen_button_remappings =
      std::move(metadata_pen_button_list);
}

}  // namespace

GraphicsTabletPrefHandlerImpl::GraphicsTabletPrefHandlerImpl() = default;
GraphicsTabletPrefHandlerImpl::~GraphicsTabletPrefHandlerImpl() = default;

void GraphicsTabletPrefHandlerImpl::InitializeGraphicsTabletSettings(
    PrefService* pref_service,
    mojom::GraphicsTablet* graphics_tablet) {
  const auto& tablet_button_remappings_dict = pref_service->GetDict(
      prefs::kGraphicsTabletTabletButtonRemappingsDictPref);
  const auto& pen_button_remappings_dict =
      pref_service->GetDict(prefs::kGraphicsTabletPenButtonRemappingsDictPref);
  const auto* tablet_button_remappings_list =
      tablet_button_remappings_dict.FindList(graphics_tablet->device_key);
  const auto* pen_button_remappings_list =
      pen_button_remappings_dict.FindList(graphics_tablet->device_key);
  mojom::GraphicsTabletSettingsPtr settings =
      mojom::GraphicsTabletSettings::New();

  // Retrieve the settings if both tablet and pen button remappings lists
  // exist.
  if (tablet_button_remappings_list && pen_button_remappings_list) {
    settings->tablet_button_remappings = ConvertListToButtonRemappingArray(
        *tablet_button_remappings_list,
        graphics_tablet->customization_restriction);
    settings->pen_button_remappings = ConvertListToButtonRemappingArray(
        *pen_button_remappings_list,
        graphics_tablet->customization_restriction);
  } else {
    settings->tablet_button_remappings = GetTabletButtonRemappingListForConfig(
        graphics_tablet->graphics_tablet_button_config);
    settings->pen_button_remappings = GetPenButtonRemappingListForConfig(
        graphics_tablet->graphics_tablet_button_config);
  }

  graphics_tablet->settings = std::move(settings);
  DCHECK(graphics_tablet->settings);

  TrimButtonRemappingListForGraphicsTabletPen(graphics_tablet);
  UpdateGraphicsTabletSettings(pref_service, *graphics_tablet);
}

void GraphicsTabletPrefHandlerImpl::UpdateGraphicsTabletSettings(
    PrefService* pref_service,
    const mojom::GraphicsTablet& graphics_tablet) {
  DCHECK(graphics_tablet.settings);
  const mojom::GraphicsTabletSettings& settings = *graphics_tablet.settings;
  const base::Time time_stamp = base::Time::Now();
  const auto time_stamp_path =
      base::StrCat({prefs::kLastUpdatedKey, ".", graphics_tablet.device_key});
  base::Value::List tablet_button_remappings =
      ConvertButtonRemappingArrayToList(
          settings.tablet_button_remappings,
          graphics_tablet.customization_restriction);
  base::Value::List pen_button_remappings = ConvertButtonRemappingArrayToList(
      settings.pen_button_remappings,
      graphics_tablet.customization_restriction);

  // Update tablet button remappings dict.
  base::Value::Dict tablet_button_remappings_dict =
      pref_service
          ->GetDict(prefs::kGraphicsTabletTabletButtonRemappingsDictPref)
          .Clone();
  tablet_button_remappings_dict.Set(graphics_tablet.device_key,
                                    std::move(tablet_button_remappings));
  tablet_button_remappings_dict.SetByDottedPath(time_stamp_path,
                                                base::TimeToValue(time_stamp));
  pref_service->SetDict(
      std::string(prefs::kGraphicsTabletTabletButtonRemappingsDictPref),
      std::move(tablet_button_remappings_dict));

  // Update pen button remappings dict.
  base::Value::Dict pen_button_remappings_dict =
      pref_service->GetDict(prefs::kGraphicsTabletPenButtonRemappingsDictPref)
          .Clone();
  pen_button_remappings_dict.Set(graphics_tablet.device_key,
                                 std::move(pen_button_remappings));
  pen_button_remappings_dict.SetByDottedPath(time_stamp_path,
                                             base::TimeToValue(time_stamp));
  pref_service->SetDict(
      std::string(prefs::kGraphicsTabletPenButtonRemappingsDictPref),
      std::move(pen_button_remappings_dict));
}

void GraphicsTabletPrefHandlerImpl::InitializeLoginScreenGraphicsTabletSettings(
    PrefService* local_state,
    const AccountId& account_id,
    mojom::GraphicsTablet* graphics_tablet) {
  // Verify if the flag is enabled.
  if (!features::IsPeripheralCustomizationEnabled()) {
    return;
  }
  CHECK(local_state);

  mojom::GraphicsTabletSettingsPtr settings =
      mojom::GraphicsTabletSettings::New();
  const auto* tablet_button_remappings_list = GetLoginScreenButtonRemappingList(
      local_state, account_id,
      prefs::kGraphicsTabletLoginScreenTabletButtonRemappingListPref);
  const auto* pen_button_remappings_list = GetLoginScreenButtonRemappingList(
      local_state, account_id,
      prefs::kGraphicsTabletLoginScreenPenButtonRemappingListPref);
  if (tablet_button_remappings_list && pen_button_remappings_list) {
    settings->tablet_button_remappings = ConvertListToButtonRemappingArray(
        *tablet_button_remappings_list,
        graphics_tablet->customization_restriction);
    settings->pen_button_remappings = ConvertListToButtonRemappingArray(
        *pen_button_remappings_list,
        graphics_tablet->customization_restriction);
  }
  graphics_tablet->settings = std::move(settings);
}

void GraphicsTabletPrefHandlerImpl::UpdateLoginScreenGraphicsTabletSettings(
    PrefService* local_state,
    const AccountId& account_id,
    const mojom::GraphicsTablet& graphics_tablet) {
  CHECK(local_state);

  user_manager::KnownUser(local_state)
      .SetPath(
          account_id,
          prefs::kGraphicsTabletLoginScreenTabletButtonRemappingListPref,
          std::make_optional<base::Value>(ConvertButtonRemappingArrayToList(
              graphics_tablet.settings->tablet_button_remappings,
              graphics_tablet.customization_restriction,
              /*redact_button_names=*/true)));
  user_manager::KnownUser(local_state)
      .SetPath(
          account_id,
          prefs::kGraphicsTabletLoginScreenPenButtonRemappingListPref,
          std::make_optional<base::Value>(ConvertButtonRemappingArrayToList(
              graphics_tablet.settings->pen_button_remappings,
              graphics_tablet.customization_restriction,
              /*redact_button_names=*/true)));
}

}  // namespace ash
