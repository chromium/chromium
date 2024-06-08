// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/birch/birch_bar_context_menu_model.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/wm/overview/overview_utils.h"
#include "base/types/cxx23_to_underlying.h"
#include "chromeos/ash/components/geolocation/simple_geolocation_provider.h"
#include "ui/base/models/menu_separator_types.h"
#include "ui/views/controls/menu/menu_types.h"

namespace ash {
namespace {

// Returns whether the weather item should be enabled based on the geolocation
// permission. See BirchWeatherProvider.
bool IsWeatherAllowedByGeolocation() {
  return SimpleGeolocationProvider::GetInstance()
      ->IsGeolocationUsageAllowedForSystem();
}

}  // namespace

BirchBarContextMenuModel::BirchBarContextMenuModel(
    ui::SimpleMenuModel::Delegate* delegate,
    Type type)
    : ui::SimpleMenuModel(delegate) {
  // Show suggestions option is in both expanded and collapsed menu.
  AddItem(base::to_underlying(CommandId::kShowSuggestions),
          u"Show suggestions");

  // Expanded menu also has customizing suggestions options.
  if (type == Type::kExpandedBarMenu) {
    AddSeparator(ui::MenuSeparatorType::NORMAL_SEPARATOR);

    AddItem(base::to_underlying(CommandId::kWeatherSuggestions), u"Weather");
    auto weather_index = GetIndexOfCommandId(
        base::to_underlying(CommandId::kWeatherSuggestions));
    bool enabled = IsWeatherAllowedByGeolocation();
    SetEnabledAt(weather_index.value(), enabled);
    if (!enabled) {
      // TODO(b/328486578): Localize string once it is finalized.
      SetMinorText(weather_index.value(),
                   u"Weather is not available because location access is "
                   u"turned off in settings");
    }

    AddItem(base::to_underlying(CommandId::kCalendarSuggestions),
            u"Google Calendar");
    AddItem(base::to_underlying(CommandId::kDriveSuggestions), u"Google Drive");
    AddItem(base::to_underlying(CommandId::kOtherDeviceSuggestions),
            u"Chrome from other devices");
    AddItem(base::to_underlying(CommandId::kLastActiveSuggestions),
            u"Last tab opened");
    AddItem(base::to_underlying(CommandId::kMostVisitedSuggestions),
            u"Frequently visited tabs");
    AddSeparator(ui::MenuSeparatorType::NORMAL_SEPARATOR);
    AddItemWithIcon(base::to_underlying(CommandId::kReset), u"Reset",
                    CreateIconForMenuItem(kResetIcon));
  }
}

BirchBarContextMenuModel::~BirchBarContextMenuModel() = default;

}  // namespace ash
