// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/pref_handlers/graphics_tablet_pref_handler_impl.h"

#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/public/mojom/input_device_settings.mojom.h"
#include "ash/system/input_device_settings/input_device_settings_pref_names.h"
#include "ash/system/input_device_settings/input_device_settings_utils.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/known_user.h"

namespace ash {

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
        mojom::CustomizationRestriction::kAllowCustomizations);
    settings->pen_button_remappings = ConvertListToButtonRemappingArray(
        *pen_button_remappings_list,
        mojom::CustomizationRestriction::kAllowCustomizations);
  }
  graphics_tablet->settings = std::move(settings);
  DCHECK(graphics_tablet->settings);

  UpdateGraphicsTabletSettings(pref_service, *graphics_tablet);
}

void GraphicsTabletPrefHandlerImpl::UpdateGraphicsTabletSettings(
    PrefService* pref_service,
    const mojom::GraphicsTablet& graphics_tablet) {
  DCHECK(graphics_tablet.settings);
  const mojom::GraphicsTabletSettings& settings = *graphics_tablet.settings;
  base::Value::List tablet_button_remappings =
      ConvertButtonRemappingArrayToList(
          settings.tablet_button_remappings,
          mojom::CustomizationRestriction::kAllowCustomizations);
  base::Value::List pen_button_remappings = ConvertButtonRemappingArrayToList(
      settings.pen_button_remappings,
      mojom::CustomizationRestriction::kAllowCustomizations);

  // Update tablet button remappings dict.
  base::Value::Dict tablet_button_remappings_dict =
      pref_service
          ->GetDict(prefs::kGraphicsTabletTabletButtonRemappingsDictPref)
          .Clone();
  tablet_button_remappings_dict.Set(graphics_tablet.device_key,
                                    std::move(tablet_button_remappings));
  pref_service->SetDict(
      std::string(prefs::kGraphicsTabletTabletButtonRemappingsDictPref),
      std::move(tablet_button_remappings_dict));

  // Update pen button remappings dict.
  base::Value::Dict pen_button_remappings_dict =
      pref_service->GetDict(prefs::kGraphicsTabletPenButtonRemappingsDictPref)
          .Clone();
  pen_button_remappings_dict.Set(graphics_tablet.device_key,
                                 std::move(pen_button_remappings));
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
        mojom::CustomizationRestriction::kAllowCustomizations);
    settings->pen_button_remappings = ConvertListToButtonRemappingArray(
        *pen_button_remappings_list,
        mojom::CustomizationRestriction::kAllowCustomizations);
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
          absl::make_optional<base::Value>(ConvertButtonRemappingArrayToList(
              graphics_tablet.settings->tablet_button_remappings,
              mojom::CustomizationRestriction::kAllowCustomizations)));
  user_manager::KnownUser(local_state)
      .SetPath(
          account_id,
          prefs::kGraphicsTabletLoginScreenPenButtonRemappingListPref,
          absl::make_optional<base::Value>(ConvertButtonRemappingArrayToList(
              graphics_tablet.settings->pen_button_remappings,
              mojom::CustomizationRestriction::kAllowCustomizations)));
}

}  // namespace ash
