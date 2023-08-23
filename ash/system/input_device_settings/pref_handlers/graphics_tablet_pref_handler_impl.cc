// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/pref_handlers/graphics_tablet_pref_handler_impl.h"

#include "ash/public/mojom/input_device_settings.mojom.h"
#include "ash/system/input_device_settings/input_device_settings_pref_names.h"
#include "ash/system/input_device_settings/input_device_settings_utils.h"
#include "base/notreached.h"
#include "components/prefs/pref_service.h"

namespace ash {

GraphicsTabletPrefHandlerImpl::GraphicsTabletPrefHandlerImpl() = default;
GraphicsTabletPrefHandlerImpl::~GraphicsTabletPrefHandlerImpl() = default;

// TODO(wangdanny): Implement graphics_tablet settings initialization.
void GraphicsTabletPrefHandlerImpl::InitializeGraphicsTabletSettings(
    PrefService* pref_service,
    mojom::GraphicsTablet* graphics_tablet) {
  NOTIMPLEMENTED();
}

void GraphicsTabletPrefHandlerImpl::UpdateGraphicsTabletSettings(
    PrefService* pref_service,
    const mojom::GraphicsTablet& graphics_tablet) {
  DCHECK(graphics_tablet.settings);
  const mojom::GraphicsTabletSettings& settings = *graphics_tablet.settings;
  base::Value::List tablet_button_remappings =
      ConvertButtonRemappingArrayToList(settings.tablet_button_remappings);
  base::Value::List pen_button_remappings =
      ConvertButtonRemappingArrayToList(settings.pen_button_remappings);

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

}  // namespace ash
