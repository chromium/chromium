// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_button_view.h"

#include <memory>
#include <string>

#include "ash/capture_mode/capture_mode_constants.h"
#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/capture_mode_session_focus_cycler.h"
#include "ash/capture_mode/capture_mode_types.h"
#include "ash/constants/ash_features.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/style_util.h"
#include "base/functional/bind.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

constexpr int kCaptureLabelRadius = 18;

// Defines the state of the capture button, which is the ID of the string used
// as its label, and its icon. These are selected based on the current state of
// capture mode, whether capture images or videos, and which video format is
// selected.
struct CaptureButtonState {
  const int label_id;
  const gfx::VectorIcon& vector_icon;
};

// Based on the current state of capture mode, returns the state with which the
// capture button should be updated.
CaptureButtonState GetCaptureButtonState() {
  const auto* const controller = CaptureModeController::Get();
  if (controller->type() == CaptureModeType::kImage) {
    return CaptureButtonState{IDS_ASH_SCREEN_CAPTURE_LABEL_IMAGE_CAPTURE,
                              kCaptureModeImageIcon};
  }

  if (controller->recording_type() == RecordingType::kWebM) {
    return CaptureButtonState{IDS_ASH_SCREEN_CAPTURE_LABEL_VIDEO_RECORD,
                              kCaptureModeVideoIcon};
  }

  DCHECK(features::IsGifRecordingEnabled());
  DCHECK_EQ(controller->recording_type(), RecordingType::kGif);

  return CaptureButtonState{IDS_ASH_SCREEN_CAPTURE_LABEL_GIF_RECORD,
                            kCaptureGifIcon};
}

// Sets up the the given `button`'s ink drop style and focus behavior.
void SetupButton(views::Button* button) {
  button->SetFocusBehavior(views::View::FocusBehavior::ACCESSIBLE_ONLY);
  views::InkDrop::Get(button)->SetMode(views::InkDropHost::InkDropMode::ON);
  button->SetHasInkDropActionOnClick(true);
  StyleUtil::ConfigureInkDropAttributes(
      button, StyleUtil::kBaseColor | StyleUtil::kInkDropOpacity);
  button->SetNotifyEnterExitOnChild(true);

  // TODO(b/266261745): Implement a custom focus ring shape that matches the
  // specs, with rounded corners on one side of each button, and sharp edges on
  // the other.
  CaptureModeSessionFocusCycler::HighlightHelper::Install(
      button,
      base::BindRepeating(
          []() -> std::unique_ptr<views::HighlightPathGenerator> {
            // Regular focus rings are drawn outside the view's bounds. Since
            // this view is the same size as its widget, inset by half the focus
            // ring thickness to ensure the focus ring is drawn inside the
            // widget bounds.
            return std::make_unique<views::RoundRectHighlightPathGenerator>(
                gfx::Insets(views::FocusRing::kDefaultHaloThickness / 2),
                kCaptureLabelRadius);
          }));
}

}  // namespace

CaptureButtonView::CaptureButtonView(
    views::Button::PressedCallback on_capture_button_pressed,
    views::Button::PressedCallback on_drop_down_pressed)
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

  const auto button_state = GetCaptureButtonState();
  capture_button_->SetText(l10n_util::GetStringUTF16(button_state.label_id));

  const SkColor icon_color =
      GetColorProvider()->GetColor(kColorAshIconColorPrimary);
  capture_button_->SetImageModel(
      views::Button::STATE_NORMAL,
      ui::ImageModel::FromVectorIcon(button_state.vector_icon, icon_color));
}

std::vector<CaptureModeSessionFocusCycler::HighlightableView*>
CaptureButtonView::GetHighlightableItems() const {
  std::vector<CaptureModeSessionFocusCycler::HighlightableView*> result{
      CaptureModeSessionFocusCycler::HighlightHelper::Get(capture_button_)};
  if (drop_down_button_ && drop_down_button_->GetVisible()) {
    result.push_back(
        CaptureModeSessionFocusCycler::HighlightHelper::Get(drop_down_button_));
  }
  return result;
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
