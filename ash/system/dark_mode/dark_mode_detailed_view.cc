// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/dark_mode/dark_mode_detailed_view.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_detailed_view.h"
#include "ash/system/tray/tray_popup_item_style.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/controls/button/radio_button.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/controls/scroll_view.h"

namespace ash {

namespace {

class TrayRadioButton : public views::RadioButton {
 public:
  TrayRadioButton(views::ButtonListener* listener,
                  const base::string16& button_label)
      : views::RadioButton(button_label) {
    SetBorder(views::CreateEmptyBorder(kTrayRadioButtonPadding));
    SetImageLabelSpacing(kTrayRadioButtonInterSpacing);
    set_callback(views::Button::PressedCallback(listener, this));
  }

  // views::RadioButton:
  void OnThemeChanged() override {
    TrayPopupItemStyle style(TrayPopupItemStyle::FontStyle::SMALL_TITLE);
    SetEnabledTextColors(style.GetTextColor());
    style.SetupLabel(label());
  }
};

}  // namespace

DarkModeDetailedView::DarkModeDetailedView(DetailedViewDelegate* delegate)
    : TrayDetailedView(delegate) {
  CreateItems();
}

DarkModeDetailedView::~DarkModeDetailedView() = default;

void DarkModeDetailedView::CreateItems() {
  CreateTitleRow(IDS_ASH_STATUS_TRAY_DARK_THEME);

  // Add toggle button.
  tri_view()->SetContainerVisible(TriView::Container::END, true);

  auto* ash_color_provider = AshColorProvider::Get();
  toggle_ =
      TrayPopupUtils::CreateToggleButton(this, IDS_ASH_STATUS_TRAY_DARK_THEME);
  toggle_->SetIsOn(ash_color_provider->IsDarkModeEnabled());
  tri_view()->AddView(TriView::Container::END, toggle_);

  // Add color mode options.
  CreateScrollableList();
  AddScrollListSubHeader(kDarkThemeColorModeIcon,
                         IDS_ASH_STATUS_TRAY_DARK_THEME_COLOR_MODE);

  themed_mode_button_ =
      scroll_content()->AddChildView(std::make_unique<TrayRadioButton>(
          this, l10n_util::GetStringUTF16(
                    IDS_ASH_STATUS_TRAY_DARK_THEME_MODE_THEMED_TITLE)));
  TrayPopupUtils::SetupTraySubLabel(scroll_content()->AddChildView(
      std::make_unique<views::Label>(l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_DARK_THEME_MODE_THEMED_DESCRIPTION))));

  neutral_mode_button_ =
      scroll_content()->AddChildView(std::make_unique<TrayRadioButton>(
          this, l10n_util::GetStringUTF16(
                    IDS_ASH_STATUS_TRAY_DARK_THEME_MODE_NEUTRAL_TITLE)));
  TrayPopupUtils::SetupTraySubLabel(scroll_content()->AddChildView(
      std::make_unique<views::Label>(l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_DARK_THEME_MODE_NEUTRAL_DESCRIPTION))));

  UpdateCheckedButton(ash_color_provider->IsThemed());
  scroll_content()->SizeToPreferredSize();
  Layout();
}

const char* DarkModeDetailedView::GetClassName() const {
  return "DarkModeDetailedView";
}

void DarkModeDetailedView::UpdateToggleButton(bool dark_mode_enabled) {
  DCHECK(toggle_);
  toggle_->AnimateIsOn(dark_mode_enabled);
}

void DarkModeDetailedView::UpdateCheckedButton(bool is_themed) {
  is_themed ? themed_mode_button_->SetChecked(true)
            : neutral_mode_button_->SetChecked(true);
}

void DarkModeDetailedView::HandleButtonPressed(views::Button* sender,
                                               const ui::Event& event) {
  auto* ash_color_provider = AshColorProvider::Get();
  if (sender == toggle_)
    ash_color_provider->ToggleColorMode();
  else if (sender == themed_mode_button_)
    ash_color_provider->UpdateColorModeThemed(/*is_themed=*/true);
  else if (sender == neutral_mode_button_)
    ash_color_provider->UpdateColorModeThemed(/*is_themed=*/false);
}

}  // namespace ash
