// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_settings_entry_view.h"

#include "ash/capture_mode/capture_mode_constants.h"
#include "ash/style/ash_color_provider.h"
#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

constexpr gfx::Size kIconSize{20, 20};

}  // namespace

CaptureModeSettingsEntryView::CaptureModeSettingsEntryView(
    views::Button::PressedCallback callback,
    const gfx::VectorIcon& icon,
    int string_id)
    : icon_view_(AddChildView(std::make_unique<views::ImageView>())),
      text_view_(AddChildView(std::make_unique<views::Label>(
          l10n_util::GetStringUTF16(string_id)))),
      toggle_button_view_(
          AddChildView(std::make_unique<views::ToggleButton>(callback))) {
  icon_view_->SetImageSize(kIconSize);
  icon_view_->SetPreferredSize(kIconSize);
  SetIcon(icon);

  auto* color_provider = AshColorProvider::Get();
  SkColor text_color = color_provider->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorPrimary);

  text_view_->SetEnabledColor(text_color);
  text_view_->SetBackgroundColor(SK_ColorTRANSPARENT);
  text_view_->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  toggle_button_view_->SetTooltipText(l10n_util::GetStringUTF16(string_id));
  toggle_button_view_->SetThumbOnColor(color_provider->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kSwitchKnobColorActive));
  toggle_button_view_->SetThumbOffColor(color_provider->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kButtonIconColor));
  toggle_button_view_->SetTrackOnColor(color_provider->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kSwitchTrackColorActive));
  toggle_button_view_->SetTrackOffColor(color_provider->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kSwitchTrackColorInactive));

  auto* box_layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
      capture_mode::kBetweenChildSpacing));
  box_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  box_layout->SetFlexForView(text_view_, 1);
}

CaptureModeSettingsEntryView::~CaptureModeSettingsEntryView() = default;

void CaptureModeSettingsEntryView::SetIcon(const gfx::VectorIcon& icon) {
  icon_view_->SetImage(gfx::CreateVectorIcon(
      icon, AshColorProvider::Get()->GetContentLayerColor(
                AshColorProvider::ContentLayerType::kButtonIconColor)));
}

views::View* CaptureModeSettingsEntryView::GetView() {
  return toggle_button_view_;
}

BEGIN_METADATA(CaptureModeSettingsEntryView, views::View)
END_METADATA

}  // namespace ash
