// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/birch/birch_bar_context_menu_model.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/wm/overview/overview_utils.h"
#include "base/types/cxx23_to_underlying.h"
#include "chromeos/ash/components/geolocation/simple_geolocation_provider.h"
#include "ui/base/l10n/l10n_util.h"
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
          l10n_util::GetStringUTF16(IDS_ASH_BIRCH_MENU_SHOW_SUGGESTIONS));

  // Expanded menu also has customizing suggestions options.
  if (type == Type::kExpandedBarMenu) {
    AddSeparator(ui::MenuSeparatorType::NORMAL_SEPARATOR);

    bool enabled = IsWeatherAllowedByGeolocation();
    std::u16string weather_label =
        enabled ? l10n_util::GetStringUTF16(IDS_ASH_BIRCH_MENU_WEATHER)
                : l10n_util::GetStringUTF16(
                      IDS_ASH_BIRCH_MENU_WEATHER_NOT_AVAILABLE);
    AddItem(base::to_underlying(CommandId::kWeatherSuggestions), weather_label);
    auto weather_index = GetIndexOfCommandId(
        base::to_underlying(CommandId::kWeatherSuggestions));
    SetEnabledAt(weather_index.value(), enabled);
    if (!enabled) {
      SetMinorText(weather_index.value(),
                   l10n_util::GetStringUTF16(
                       IDS_ASH_BIRCH_MENU_WEATHER_NOT_AVAILABLE_TOOLTIP));
    }

    AddItem(base::to_underlying(CommandId::kCalendarSuggestions),
            l10n_util::GetStringUTF16(IDS_ASH_BIRCH_MENU_CALENDAR));
    AddItem(base::to_underlying(CommandId::kDriveSuggestions),
            l10n_util::GetStringUTF16(IDS_ASH_BIRCH_MENU_DRIVE));
    AddItem(base::to_underlying(CommandId::kChromeTabSuggestions),
            l10n_util::GetStringUTF16(IDS_ASH_BIRCH_MENU_CHROME_BROWSER));
    AddItem(base::to_underlying(CommandId::kMediaSuggestions),
            l10n_util::GetStringUTF16(IDS_ASH_BIRCH_MENU_MEDIA));
    // TODO(yulunwu) Replace with product name.
    AddItem(base::to_underlying(CommandId::kCoralSuggestions), u"Coral");
    AddSeparator(ui::MenuSeparatorType::NORMAL_SEPARATOR);
    AddItemWithIcon(base::to_underlying(CommandId::kReset),
                    l10n_util::GetStringUTF16(IDS_ASH_BIRCH_MENU_RESET),
                    CreateIconForMenuItem(kResetIcon));
  }
}

BirchBarContextMenuModel::~BirchBarContextMenuModel() = default;

}  // namespace ash
