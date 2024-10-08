// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/birch/birch_bar_util.h"

#include "ash/strings/grit/ash_strings.h"
#include "ash/style/icon_button.h"
#include "ash/style/pill_button.h"
#include "ash/style/typography.h"
#include "ash/wm/overview/birch/birch_bar_context_menu_model.h"
#include "ash/wm/overview/birch/birch_bar_controller.h"
#include "ash/wm/overview/birch/birch_bar_view.h"
#include "ash/wm/overview/birch/birch_chip_button.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"

namespace ash::birch_bar_util {

namespace {

// The layout parameters of add-ons.
constexpr gfx::Insets kAddonMargins = gfx::Insets::VH(0, 16);
constexpr int kWeatherTempLabelSpacing = 2;

// The font of add-on label.
constexpr TypographyToken kWeatherTempLabelFont =
    TypographyToken::kCrosDisplay3Regular;
constexpr TypographyToken kWeatherUnitLabelFont = TypographyToken::kCrosTitle1;
}  // namespace

std::unique_ptr<views::Button> CreateAddonButton(
    views::Button::PressedCallback callback,
    const std::u16string& label) {
  auto button = std::make_unique<PillButton>(
      std::move(callback), label, PillButton::Type::kSecondaryWithoutIcon);
  button->SetProperty(views::kMarginsKey, kAddonMargins);
  return button;
}

std::unique_ptr<views::Button> CreateCoralAddonButton(
    views::Button::PressedCallback callback,
    const gfx::VectorIcon& button_icon,
    const std::u16string& accessible_name) {
  auto button = std::make_unique<IconButton>(
      std::move(callback), IconButton::Type::kMediumProminent, &button_icon,
      accessible_name, /*is_togglable=*/true, /*has_border=*/true);
  button->SetProperty(views::kMarginsKey, kAddonMargins);
  button->SetBackgroundColor(cros_tokens::kCrosSysSystemBaseElevated);
  button->SetIconColor(cros_tokens::kCrosSysSecondary);
  return button;
}

std::unique_ptr<views::View> CreateWeatherTemperatureView(
    const std::u16string& temp_str,
    bool fahrenheit) {
  views::Label* temp = nullptr;
  views::Label* unit = nullptr;

  auto weather_view =
      views::Builder<views::BoxLayoutView>()
          .SetBetweenChildSpacing(kWeatherTempLabelSpacing)
          .SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kStart)
          .SetProperty(views::kMarginsKey, kAddonMargins)
          .SetFocusBehavior(views::View::FocusBehavior::NEVER)
          .AddChildren(
              views::Builder<views::Label>().CopyAddressTo(&temp).SetText(
                  temp_str),
              views::Builder<views::Label>().CopyAddressTo(&unit).SetText(
                  l10n_util::GetStringUTF16(
                      fahrenheit ? IDS_ASH_BIRCH_WEATHER_FAHREHEIT_SYMBOL
                                 : IDS_ASH_BIRCH_WEATHER_CELSIUS_SYMBOL)))
          .Build();

  auto* typography_provider = TypographyProvider::Get();
  typography_provider->StyleLabel(kWeatherTempLabelFont, *temp);
  typography_provider->StyleLabel(kWeatherUnitLabelFont, *unit);

  return weather_view;
}

BirchSuggestionType CommandIdToSuggestionType(int command_id) {
  using CommandId = BirchBarContextMenuModel::CommandId;
  switch (command_id) {
    case base::to_underlying(CommandId::kCalendarSuggestions):
      return BirchSuggestionType::kCalendar;
    case base::to_underlying(CommandId::kWeatherSuggestions):
      return BirchSuggestionType::kWeather;
    case base::to_underlying(CommandId::kDriveSuggestions):
      return BirchSuggestionType::kDrive;
    case base::to_underlying(CommandId::kChromeTabSuggestions):
      return BirchSuggestionType::kChromeTab;
    case base::to_underlying(CommandId::kMediaSuggestions):
      return BirchSuggestionType::kMedia;
    case base::to_underlying(CommandId::kCoralSuggestions):
      return BirchSuggestionType::kCoral;
    default:
      break;
  }
  NOTREACHED_NORETURN() << "No matching suggestion type for command Id: "
                        << command_id;
}

TabAppSelectionHost* GetVisibleTabAppSelectionHost() {
  auto* birch_bar_controller = BirchBarController::Get();
  if (!birch_bar_controller) {
    return nullptr;
  }

  const std::vector<raw_ptr<BirchBarView>> bar_views =
      birch_bar_controller->bar_views();
  for (BirchBarView* bar_view : bar_views) {
    auto iter =
        base::ranges::find_if(bar_view->chips(), [](BirchChipButtonBase* chip) {
          if (!views::IsViewClass<BirchChipButton>(chip)) {
            return false;
          }

          TabAppSelectionHost* selection_host =
              views::AsViewClass<BirchChipButton>(chip)
                  ->tab_app_selection_widget();
          return selection_host && selection_host->IsVisible();
        });
    if (iter != bar_view->chips().end()) {
      return views::AsViewClass<BirchChipButton>(*iter)
          ->tab_app_selection_widget();
    }
  }
  return nullptr;
}

}  // namespace ash::birch_bar_util
