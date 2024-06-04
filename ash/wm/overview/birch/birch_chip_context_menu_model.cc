// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/birch/birch_chip_context_menu_model.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/wm/overview/overview_utils.h"
#include "ui/base/models/menu_separator_types.h"
#include "ui/views/controls/menu/menu_types.h"

namespace ash {

BirchChipContextMenuModel::BirchChipContextMenuModel(
    ui::SimpleMenuModel::Delegate* delegate,
    BirchSuggestionType chip_type)
    : ui::SimpleMenuModel(delegate),
      sub_menu_model_(std::make_unique<BirchBarContextMenuModel>(
          delegate,
          BirchBarContextMenuModel::Type::kExpandedBarMenu)) {
  auto add_hide_suggestion_item = [&]() {
    AddItemWithIcon(base::to_underlying(CommandId::kHideSuggestion),
                    u"Hide this suggestion",
                    CreateIconForMenuItem(kSystemTrayDoNotDisturbIcon));
  };

  switch (chip_type) {
    case BirchSuggestionType::kWeather:
      AddItemWithIcon(base::to_underlying(CommandId::kHideWeatherSuggestions),
                      u"Hide Weather suggestion",
                      CreateIconForMenuItem(kForbidIcon));
      break;
    case BirchSuggestionType::kCalendar:
      add_hide_suggestion_item();
      AddItemWithIcon(base::to_underlying(CommandId::kHideCalendarSuggestions),
                      u"Hide all Google Calendar suggestions",
                      CreateIconForMenuItem(kForbidIcon));
      break;
    case BirchSuggestionType::kDrive:
      add_hide_suggestion_item();
      AddItemWithIcon(base::to_underlying(CommandId::kHideDriveSuggestions),
                      u"Hide all Google Drive suggestions",
                      CreateIconForMenuItem(kForbidIcon));
      break;
    case BirchSuggestionType::kTab:
      add_hide_suggestion_item();
      AddItemWithIcon(
          base::to_underlying(CommandId::kHideOtherDeviceSuggestions),
          u"Hide all Chrome suggestions", CreateIconForMenuItem(kForbidIcon));
      break;
    case BirchSuggestionType::kLastActive:
      add_hide_suggestion_item();
      AddItemWithIcon(
          base::to_underlying(CommandId::kHideLastActiveSuggestions),
          u"Hide last tab opened suggestions",
          CreateIconForMenuItem(kForbidIcon));
      break;
    case BirchSuggestionType::kMostVisited:
      add_hide_suggestion_item();
      AddItemWithIcon(
          base::to_underlying(CommandId::kHideMostVisitedSuggestions),
          u"Hide frequently visited tab suggestions",
          CreateIconForMenuItem(kForbidIcon));
      break;
    default:
      break;
  }

  AddSubMenuWithIcon(base::to_underlying(CommandId::kCustomizeSuggestions),
                     u"Customize suggestions", sub_menu_model_.get(),
                     CreateIconForMenuItem(kPencilIcon));
  if (chip_type == BirchSuggestionType::kWeather) {
    AddItem(base::to_underlying(CommandId::kToggleTemperatureUnits),
            u"Toggle temperature units (F vs C)");
  }
  AddSeparator(ui::MenuSeparatorType::NORMAL_SEPARATOR);
  AddItemWithIcon(base::to_underlying(CommandId::kFeedback), u"Send Feedback",
                  CreateIconForMenuItem(kFeedbackIcon));
}

BirchChipContextMenuModel::~BirchChipContextMenuModel() = default;

}  // namespace ash
