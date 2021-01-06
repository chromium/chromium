// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/dark_mode/dark_mode_detailed_view.h"
#include <cstddef>

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_detailed_view.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/controls/button/radio_button.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/metadata/metadata_impl_macros.h"

namespace ash {

namespace {

class TrayRadioButton : public views::RadioButton {
 public:
  TrayRadioButton(PressedCallback callback, const base::string16& button_label)
      : views::RadioButton(button_label) {
    SetCallback(std::move(callback));
    SetBorder(views::CreateEmptyBorder(kTrayRadioButtonPadding));
    SetImageLabelSpacing(kTrayRadioButtonInterSpacing);
  }

  // views::RadioButton:
  SkColor GetIconImageColor(int icon_state) const override {
    return AshColorProvider::Get()->GetContentLayerColor(
        icon_state & IconState::CHECKED
            ? AshColorProvider::ContentLayerType::kRadioColorActive
            : AshColorProvider::ContentLayerType::kRadioColorInactive);
  }

  // views::RadioButton:
  void OnThemeChanged() override {
    views::RadioButton::OnThemeChanged();
    SetEnabledTextColors(AshColorProvider::Get()->GetContentLayerColor(
        AshColorProvider::ContentLayerType::kTextColorPrimary));
    TrayPopupUtils::SetLabelFontList(label(),
                                     TrayPopupUtils::FontStyle::kSmallTitle);
  }
};

SkColor GetLabelColor() {
  return AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorSecondary);
}

void SetupLabel(views::Label* label) {
  label->SetBorder(views::CreateEmptyBorder(kTraySubLabelPadding));
  label->SetMultiLine(true);
  label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  label->SetEnabledColor(GetLabelColor());
}

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
  toggle_ = TrayPopupUtils::CreateToggleButton(
      base::BindRepeating(&AshColorProvider::ToggleColorMode,
                          base::Unretained(AshColorProvider::Get())),
      IDS_ASH_STATUS_TRAY_DARK_THEME);
  toggle_->SetIsOn(ash_color_provider->IsDarkModeEnabled());
  tri_view()->AddView(TriView::Container::END, toggle_);

  // Add color mode options.
  CreateScrollableList();
  AddScrollListSubHeader(kDarkThemeColorModeIcon,
                         IDS_ASH_STATUS_TRAY_DARK_THEME_COLOR_MODE);

  themed_mode_button_ =
      scroll_content()->AddChildView(std::make_unique<TrayRadioButton>(
          base::BindRepeating(&AshColorProvider::UpdateColorModeThemed,
                              base::Unretained(AshColorProvider::Get()), true),
          l10n_util::GetStringUTF16(
              IDS_ASH_STATUS_TRAY_DARK_THEME_MODE_THEMED_TITLE)));
  themed_label_ = scroll_content()->AddChildView(
      std::make_unique<views::Label>(l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_DARK_THEME_MODE_THEMED_DESCRIPTION)));
  SetupLabel(themed_label_);

  neutral_mode_button_ =
      scroll_content()->AddChildView(std::make_unique<TrayRadioButton>(
          base::BindRepeating(&AshColorProvider::UpdateColorModeThemed,
                              base::Unretained(AshColorProvider::Get()), false),
          l10n_util::GetStringUTF16(
              IDS_ASH_STATUS_TRAY_DARK_THEME_MODE_NEUTRAL_TITLE)));
  neutral_label_ = scroll_content()->AddChildView(
      std::make_unique<views::Label>(l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_DARK_THEME_MODE_NEUTRAL_DESCRIPTION)));
  SetupLabel(neutral_label_);

  UpdateCheckedButton(ash_color_provider->IsThemed());
  scroll_content()->SizeToPreferredSize();
  Layout();
}

void DarkModeDetailedView::OnThemeChanged() {
  TrayDetailedView::OnThemeChanged();
  TrayPopupUtils::SetLabelFontList(themed_label_,
                                   TrayPopupUtils::FontStyle::kSystemInfo);
  TrayPopupUtils::SetLabelFontList(neutral_label_,
                                   TrayPopupUtils::FontStyle::kSystemInfo);
  TrayPopupUtils::UpdateToggleButtonColors(toggle_);
}

void DarkModeDetailedView::UpdateToggleButton(bool dark_mode_enabled) {
  DCHECK(toggle_);
  toggle_->AnimateIsOn(dark_mode_enabled);
}

void DarkModeDetailedView::UpdateCheckedButton(bool is_themed) {
  is_themed ? themed_mode_button_->SetChecked(true)
            : neutral_mode_button_->SetChecked(true);
}

BEGIN_METADATA(DarkModeDetailedView, TrayDetailedView)
END_METADATA

}  // namespace ash
