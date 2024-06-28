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

    bool enabled = IsWeatherAllowedByGeolocation();
    AddItem(base::to_underlying(CommandId::kWeatherSuggestions),
            enabled ? u"Weather" : u"Weather (not available)");
    auto weather_index = GetIndexOfCommandId(
        base::to_underlying(CommandId::kWeatherSuggestions));
    SetEnabledAt(weather_index.value(), enabled);
    if (!enabled) {
      // TODO(b/328486578): Localize string.
      SetMinorText(weather_index.value(),
                   u"Weather isn't available because location access is "
                   u"turned off. You can change this in settings.");
    }

    AddItem(base::to_underlying(CommandId::kCalendarSuggestions),
            u"Google Calendar");
    AddItem(base::to_underlying(CommandId::kDriveSuggestions), u"Google Drive");
    AddItem(base::to_underlying(CommandId::kChromeTabSuggestions),
            u"Chrome browser");
    AddSeparator(ui::MenuSeparatorType::NORMAL_SEPARATOR);
    AddItemWithIcon(base::to_underlying(CommandId::kReset), u"Reset",
                    CreateIconForMenuItem(kResetIcon));
  }
}

BirchBarContextMenuModel::~BirchBarContextMenuModel() = default;

}  // namespace ash
