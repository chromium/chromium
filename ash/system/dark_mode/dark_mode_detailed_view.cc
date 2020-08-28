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
    set_listener(listener);
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
  CreateTitleRow(IDS_ASH_STATUS_TRAY_DARK_THEME_TITLE);

  // Add toggle button.
  tri_view()->SetContainerVisible(TriView::Container::END, true);

  toggle_ =
      TrayPopupUtils::CreateToggleButton(this, IDS_ASH_STATUS_TRAY_BLUETOOTH);
  toggle_->SetIsOn(AshColorProvider::Get()->IsDarkModeEnabled());
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

  // Set the relevant radio button to be checked.
  AshColorProvider::Get()->is_themed() ? themed_mode_button_->SetChecked(true)
                                       : neutral_mode_button_->SetChecked(true);

  scroll_content()->SizeToPreferredSize();
  Layout();
}

const char* DarkModeDetailedView::GetClassName() const {
  return "DarkModeDetailedView";
}

void DarkModeDetailedView::HandleButtonPressed(views::Button* sender,
                                               const ui::Event& event) {
  if (sender == toggle_) {
    // TODO(amehfooz): Toggle Dark / Light mode here.
  } else if (sender == themed_mode_button_) {
    // TODO(amehfooz): Switch to themed mode here.
  } else if (sender == neutral_mode_button_) {
    // TODO(amehfooz): Switch to neutral mode here.
  }
}

}  // namespace ash
