// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/birch/birch_chip_context_menu_model.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/wm/overview/overview_utils.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/menu_separator_types.h"
#include "ui/views/controls/menu/menu_types.h"

namespace ash {
namespace {

// Returns the pref service to use for Birch bar prefs.
PrefService* GetPrefService() {
  return Shell::Get()->session_controller()->GetPrimaryUserPrefService();
}

}  // namespace

BirchChipContextMenuModel::BirchChipContextMenuModel(
    ui::SimpleMenuModel::Delegate* delegate,
    BirchSuggestionType chip_type)
    : ui::SimpleMenuModel(delegate),
      sub_menu_model_(std::make_unique<BirchBarContextMenuModel>(
          delegate,
          BirchBarContextMenuModel::Type::kExpandedBarMenu)) {
  auto add_hide_suggestion_item = [&]() {
    AddItemWithIcon(
        base::to_underlying(CommandId::kHideSuggestion),
        l10n_util::GetStringUTF16(IDS_ASH_BIRCH_HIDE_THIS_SUGGESTION),
        CreateIconForMenuItem(kSystemTrayDoNotDisturbIcon));
  };

  switch (chip_type) {
    case BirchSuggestionType::kWeather:
      AddItemWithIcon(
          base::to_underlying(CommandId::kHideWeatherSuggestions),
          l10n_util::GetStringUTF16(IDS_ASH_BIRCH_HIDE_WEATHER_SUGGESTION),
          CreateIconForMenuItem(kForbidIcon));
      break;
    case BirchSuggestionType::kCalendar:
      add_hide_suggestion_item();
      AddItemWithIcon(
          base::to_underlying(CommandId::kHideCalendarSuggestions),
          l10n_util::GetStringUTF16(IDS_ASH_BIRCH_HIDE_CALENDAR_SUGGESTIONS),
          CreateIconForMenuItem(kForbidIcon));
      break;
    case BirchSuggestionType::kDrive:
      add_hide_suggestion_item();
      AddItemWithIcon(
          base::to_underlying(CommandId::kHideDriveSuggestions),
          l10n_util::GetStringUTF16(IDS_ASH_BIRCH_HIDE_DRIVE_SUGGESTIONS),
          CreateIconForMenuItem(kForbidIcon));
      break;
    case BirchSuggestionType::kChromeTab:
      add_hide_suggestion_item();
      AddItemWithIcon(
          base::to_underlying(CommandId::kHideChromeTabSuggestions),
          l10n_util::GetStringUTF16(IDS_ASH_BIRCH_HIDE_CHROME_SUGGESTIONS),
          CreateIconForMenuItem(kForbidIcon));
      break;
    case BirchSuggestionType::kMedia:
      // There's no "Hide this suggestion" for media because the media URLs we
      // have in the BirchItem are very generic (e.g. "youtube.com"), so hiding
      // "this" suggestion would hide all media from a given site.
      AddItemWithIcon(
          base::to_underlying(CommandId::kHideMediaSuggestions),
          l10n_util::GetStringUTF16(IDS_ASH_BIRCH_HIDE_MEDIA_SUGGESTIONS),
          CreateIconForMenuItem(kForbidIcon));
      break;
    case BirchSuggestionType::kCoral:
      AddItemWithIcon(base::to_underlying(CommandId::kCoralNewDesk),
                      u"New Coral Desk", CreateIconForMenuItem(kCoralOpenIcon));
      AddItemWithIcon(base::to_underlying(CommandId::kCoralSaveForLater),
                      u"Save Coral For Later",
                      CreateIconForMenuItem(kSaveDeskForLaterIcon));
      AddSeparator(ui::NORMAL_SEPARATOR);
      add_hide_suggestion_item();
      // TODO(yulunwu) Replace with product name.
      AddItemWithIcon(base::to_underlying(CommandId::kHideCoralSuggestions),
                      u"Hide all coral suggestions",
                      CreateIconForMenuItem(kForbidIcon));
      break;
    default:
      break;
  }

  AddSubMenuWithIcon(
      base::to_underlying(CommandId::kCustomizeSuggestions),
      l10n_util::GetStringUTF16(IDS_ASH_BIRCH_CUSTOMIZE_SUGGESTIONS),
      sub_menu_model_.get(), CreateIconForMenuItem(kPencilIcon));

  if (chip_type == BirchSuggestionType::kWeather) {
    bool is_celsius = GetPrefService()->GetBoolean(prefs::kBirchUseCelsius);
    AddItem(base::to_underlying(CommandId::kToggleTemperatureUnits),
            l10n_util::GetStringUTF16(
                is_celsius ? IDS_ASH_BIRCH_SHOW_TEMPERATURE_IN_FAHRENHEIT
                           : IDS_ASH_BIRCH_SHOW_TEMPERATURE_IN_CELSIUS));
  }

  // Add feedback menu for Coral
  if (chip_type == BirchSuggestionType::kCoral) {
    AddSeparator(ui::NORMAL_SEPARATOR);
    AddItemWithIcon(base::to_underlying(CommandId::kProvideFeedback),
                    u"Send feedback", CreateIconForMenuItem(kFeedbackIcon));
  }
}

BirchChipContextMenuModel::~BirchChipContextMenuModel() = default;

}  // namespace ash
