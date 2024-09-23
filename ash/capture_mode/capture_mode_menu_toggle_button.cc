// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_menu_toggle_button.h"

#include "ash/capture_mode/capture_mode_constants.h"
#include "ash/capture_mode/capture_mode_session_focus_cycler.h"
#include "ash/capture_mode/capture_mode_util.h"
#include "ash/style/ash_color_id.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

constexpr int kSpaceBetweenChildView = 16;

}  // namespace

CaptureModeMenuToggleButton::CaptureModeMenuToggleButton(
    const gfx::VectorIcon& icon,
    const std::u16string& label_text,
    bool enabled,
    views::ToggleButton::PressedCallback callback)
    : icon_view_(AddChildView(std::make_unique<views::ImageView>())),
      label_view_(AddChildView(std::make_unique<views::Label>(label_text))),
      toggle_button_(
          AddChildView(std::make_unique<Switch>(std::move(callback)))) {
  toggle_button_->GetViewAccessibility().SetName(label_text);
  CaptureModeSessionFocusCycler::HighlightHelper::Install(toggle_button_);
  icon_view_->SetImageSize(capture_mode::kSettingsIconSize);
  icon_view_->SetPreferredSize(capture_mode::kSettingsIconSize);
  icon_view_->SetImage(
      ui::ImageModel::FromVectorIcon(icon, kColorAshButtonIconColor));
  toggle_button_->SetIsOn(enabled);

  SetBorder(views::CreateEmptyBorder(capture_mode::kSettingsMenuBorderSize));
  capture_mode_util::ConfigLabelView(label_view_);
  auto* box_layout = capture_mode_util::CreateAndInitBoxLayoutForView(this);
  box_layout->SetFlexForView(label_view_, 1);
  box_layout->set_between_child_spacing(kSpaceBetweenChildView);
}

CaptureModeMenuToggleButton::~CaptureModeMenuToggleButton() = default;

void CaptureModeMenuToggleButton::OnThemeChanged() {
  views::View::OnThemeChanged();
  auto* color_provider = GetColorProvider();
  toggle_button_->SetThumbOnColor(
      color_provider->GetColor(kColorAshSwitchKnobColorActive));
  toggle_button_->SetThumbOffColor(
      color_provider->GetColor(kColorAshSwitchKnobColorInactive));
  toggle_button_->SetTrackOnColor(
      color_provider->GetColor(kColorAshSwitchTrackColorActive));
  toggle_button_->SetTrackOffColor(
      color_provider->GetColor(kColorAshSwitchTrackColorInactive));
}

BEGIN_METADATA(CaptureModeMenuToggleButton)
END_METADATA

}  // namespace ash