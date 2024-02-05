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
#include "ash/capture_mode/capture_mode_util.h"
#include "ash/constants/ash_features.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/style_util.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
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

// The `capture_button_` can be fully rounded or half rounded depending on
// whether the `drop_down_button_` is visible or not.
constexpr gfx::RoundedCornersF kCaptureButtonFullyRoundedCorners(18);
constexpr gfx::RoundedCornersF kCaptureButtonHalfRoundedCorners(18, 0, 0, 18);
// However, the `drop_down_button_` is always half rounded.
constexpr gfx::RoundedCornersF kDropDownButtonRoundedCorners(0, 18, 18, 0);

// Regular focus rings are drawn outside the view's bounds such that there is a
// gap between the view and its focus ring. However, the
// `capture_label_widget_` tightly contains its contents view, meaning that the
// size of the widget is the same as the size of the `capture_label_view_`. If
// we outset the focus rings of `capture_button_` and `drop_down_button_`, they
// will be masked by the widget's bounds, and won't show. Hence, we inset by
// half the focus ring default thickness to ensure the focus ring is drawn
// inside the widget bounds.
constexpr gfx::Insets kFocusRingPathInsets(
    views::FocusRing::kDefaultHaloThickness / 2);

// Defines the state of the capture button, which is the ID of the string used
// as its label, and its icon. These are selected based on the current state of
// capture mode, whether capture images or videos, and which video format is
// selected.
struct CaptureButtonState {
  const int label_id;
  const raw_ref<const gfx::VectorIcon> vector_icon;
};

// Based on the current state of capture mode, returns the state with which the
// capture button should be updated.
CaptureButtonState GetCaptureButtonState() {
  const auto* const controller = CaptureModeController::Get();
  if (controller->type() == CaptureModeType::kImage) {
    return CaptureButtonState{IDS_ASH_SCREEN_CAPTURE_LABEL_IMAGE_CAPTURE,
                              ToRawRef(kCaptureModeImageIcon)};
  }

  if (controller->recording_type() == RecordingType::kWebM) {
    return CaptureButtonState{IDS_ASH_SCREEN_CAPTURE_LABEL_VIDEO_RECORD,
                              ToRawRef(kCaptureModeVideoIcon)};
  }

  DCHECK(features::IsGifRecordingEnabled());
  DCHECK_EQ(controller->recording_type(), RecordingType::kGif);

  return CaptureButtonState{IDS_ASH_SCREEN_CAPTURE_LABEL_GIF_RECORD,
                            ToRawRef(kCaptureGifIcon)};
}

}  // namespace

CaptureButtonView::CaptureButtonView(
    views::Button::PressedCallback on_capture_button_pressed,
    views::Button::PressedCallback on_drop_down_pressed,
    CaptureModeBehavior* active_behavior)
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

  // Only show the drop down button if there are more than one recording types
  // that are currently supported in the current mode (i.e. we don't bother to
  // show a drop down for a single item).
  if (active_behavior->GetSupportedRecordingTypes().size() > 1u) {
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

  // The path of the capture button's focus ring may need to change if we switch
  // from a single button to a dual button. We'll use a change in the visibility
  // of the separator as an indicator for the need to re-install a new highlight
  // path generator on the capture button.
  bool should_invalidate_focus_ring = false;

  // The recording type selection views are visible only when the capture type
  // is video recording.
  const bool is_capturing_image =
      CaptureModeController::Get()->type() == CaptureModeType::kImage;
  if (drop_down_button_) {
    DCHECK(separator_);
    const bool new_visibility = !is_capturing_image;
    should_invalidate_focus_ring = new_visibility != separator_->GetVisible();
    separator_->SetVisible(new_visibility);
    drop_down_button_->SetVisible(new_visibility);
  }

  const auto button_state = GetCaptureButtonState();
  capture_button_->SetText(l10n_util::GetStringUTF16(button_state.label_id));

  const SkColor icon_color =
      GetColorProvider()->GetColor(kColorAshIconColorPrimary);
  capture_button_->SetImageModel(
      views::Button::STATE_NORMAL,
      ui::ImageModel::FromVectorIcon(*button_state.vector_icon, icon_color));

  if (should_invalidate_focus_ring) {
    // Note that we don't need to invalidate the focus ring of the
    // `drop_down_button_` as it never changes, and always remains half rounded.
    CaptureModeSessionFocusCycler::HighlightHelper::Get(capture_button_)
        ->InvalidateFocusRingPath();

    // The ink drop highlight needs to be updated as well, since the rounded
    // corners have changed.
    views::HighlightPathGenerator::Install(
        capture_button_,
        CreateFocusRingPath(capture_button_, /*use_zero_insets=*/true));
  }
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

void CaptureButtonView::SetupButton(views::Button* button) {
  button->SetFocusBehavior(views::View::FocusBehavior::ACCESSIBLE_ONLY);
  views::InkDrop::Get(button)->SetMode(views::InkDropHost::InkDropMode::ON);
  button->SetHasInkDropActionOnClick(true);
  StyleUtil::ConfigureInkDropAttributes(
      button, StyleUtil::kBaseColor | StyleUtil::kInkDropOpacity);
  button->SetNotifyEnterExitOnChild(true);

  // This installs a path generator that will be used for the ink drop
  // highlight. It should not have any insets as the highlight should span the
  // entire bounds of the view.
  views::HighlightPathGenerator::Install(
      button, CreateFocusRingPath(button, /*use_zero_insets=*/true));

  // This will be used to install a path generator for the focus ring, which
  // should be insetted a little so that the focus ring can paint within the
  // bounds the view.
  CaptureModeSessionFocusCycler::HighlightHelper::Install(
      button, base::BindRepeating(&CaptureButtonView::CreateFocusRingPath,
                                  base::Unretained(this), button,
                                  /*use_zero_insets=*/false));

  StyleUtil::SetUpInkDropForButton(button);
}

std::unique_ptr<views::HighlightPathGenerator>
CaptureButtonView::CreateFocusRingPath(views::View* view,
                                       bool use_zero_insets) {
  const auto insets = use_zero_insets ? gfx::Insets() : kFocusRingPathInsets;
  if (view == capture_button_) {
    const bool should_ring_be_half_rounded =
        drop_down_button_ && drop_down_button_->GetVisible();
    return std::make_unique<views::RoundRectHighlightPathGenerator>(
        insets, should_ring_be_half_rounded
                    ? kCaptureButtonHalfRoundedCorners
                    : kCaptureButtonFullyRoundedCorners);
  }

  DCHECK_EQ(view, drop_down_button_);
  return std::make_unique<views::RoundRectHighlightPathGenerator>(
      insets, kDropDownButtonRoundedCorners);
}

BEGIN_METADATA(CaptureButtonView)
END_METADATA

}  // namespace ash
