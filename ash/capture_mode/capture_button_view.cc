// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_button_view.h"

#include <memory>
#include <string>

#include "ash/capture_mode/capture_mode_constants.h"
#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/capture_mode_types.h"
#include "ash/constants/ash_features.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/style_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

// Sets up the the given `button`'s ink drop style and focus behavior.
void SetupButton(views::Button* button) {
  button->SetFocusBehavior(views::View::FocusBehavior::ACCESSIBLE_ONLY);
  views::InkDrop::Get(button)->SetMode(views::InkDropHost::InkDropMode::ON);
  button->SetHasInkDropActionOnClick(true);
  StyleUtil::ConfigureInkDropAttributes(
      button, StyleUtil::kBaseColor | StyleUtil::kInkDropOpacity);
  button->SetNotifyEnterExitOnChild(true);
}

}  // namespace

CaptureButtonView::CaptureButtonView(
    base::RepeatingClosure on_capture_button_pressed,
    base::RepeatingClosure on_drop_down_pressed)
    : capture_button_(AddChildView(std::make_unique<views::LabelButton>(
          std::move(on_capture_button_pressed),
          std::u16string()))) {
  auto* box_layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
      /*between_child_spacing=*/0));
  box_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStretch);

  capture_button_->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  capture_button_->SetBorder(views::CreateEmptyBorder(gfx::Insets::VH(0, 12)));
  SetupButton(capture_button_);
  if (features::IsGifRecordingEnabled()) {
    separator_ = AddChildView(std::make_unique<views::Separator>());
    separator_->SetColorId(ui::kColorAshSystemUIMenuSeparator);
    drop_down_button_ = AddChildView(
        std::make_unique<views::ImageButton>(std::move(on_drop_down_pressed)));
    SetupButton(drop_down_button_);
    drop_down_button_->SetBorder(
        views::CreateEmptyBorder(gfx::Insets::TLBR(0, 6, 0, 8)));
    drop_down_button_->SetImageHorizontalAlignment(
        views::ImageButton::ALIGN_CENTER);
    drop_down_button_->SetImageVerticalAlignment(
        views::ImageButton::ALIGN_MIDDLE);
    drop_down_button_->SetMinimumImageSize(capture_mode::kSettingsIconSize);
    drop_down_button_->SetTooltipText(l10n_util::GetStringUTF16(
        IDS_ASH_SCREEN_CAPTURE_RECORDING_TYPE_BUTTON_TOOLTIP));
  }
}

void CaptureButtonView::UpdateViewVisuals() {
  // This view should be visible only if we're capturing a non-empty region.
  DCHECK(GetVisible());

  // The recording type selection views are visible only when the capture type
  // is video recording.
  const bool is_capturing_image =
      CaptureModeController::Get()->type() == CaptureModeType::kImage;
  if (features::IsGifRecordingEnabled()) {
    separator_->SetVisible(!is_capturing_image);
    drop_down_button_->SetVisible(!is_capturing_image);
  }

  capture_button_->SetText(l10n_util::GetStringUTF16(
      is_capturing_image ? IDS_ASH_SCREEN_CAPTURE_LABEL_IMAGE_CAPTURE
                         : IDS_ASH_SCREEN_CAPTURE_LABEL_VIDEO_RECORD));

  const SkColor icon_color =
      GetColorProvider()->GetColor(kColorAshIconColorPrimary);
  capture_button_->SetImageModel(
      views::Button::STATE_NORMAL,
      ui::ImageModel::FromVectorIcon(
          is_capturing_image ? kCaptureModeImageIcon : kCaptureModeVideoIcon,
          icon_color));
}

void CaptureButtonView::OnThemeChanged() {
  views::View::OnThemeChanged();

  auto* color_provider = GetColorProvider();
  capture_button_->SetEnabledTextColors(
      color_provider->GetColor(kColorAshTextColorPrimary));

  if (drop_down_button_) {
    drop_down_button_->SetImageModel(
        views::Button::STATE_NORMAL,
        ui::ImageModel::FromVectorIcon(
            kDropDownArrowIcon,
            color_provider->GetColor(kColorAshIconColorPrimary)));
  }
}

BEGIN_METADATA(CaptureButtonView, views::View)
END_METADATA

}  // namespace ash
