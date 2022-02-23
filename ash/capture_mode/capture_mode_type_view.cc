// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_type_view.h"

#include <memory>

#include "ash/capture_mode/capture_mode_camera_controller.h"
#include "ash/capture_mode/capture_mode_constants.h"
#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/capture_mode_metrics.h"
#include "ash/capture_mode/capture_mode_toggle_button.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "base/bind.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

constexpr int kBackgroundCornerRadius = 18;

constexpr gfx::Insets kViewInsets{2};

// If `projector_mode` is false, Creates and initializes the toggle button that
// is used for switching to image capture as a child of the given
// `capture_mode_type_view` and returns a pointer to it.
// `on_image_toggle_method` is a pointer to a member function inside
// `CaptureModeTypeView` which will be triggered when the image toggle button is
// pressed. Returns nullptr if `projector_mode` is true.
CaptureModeToggleButton* MaybeCreateImageToggleButton(
    CaptureModeTypeView* capture_mode_type_view,
    void (CaptureModeTypeView::*on_image_toggle_method)(),
    bool projector_mode) {
  if (projector_mode)
    return nullptr;

  CaptureModeToggleButton* image_toggle_button =
      capture_mode_type_view->AddChildView(
          std::make_unique<CaptureModeToggleButton>(
              base::BindRepeating(on_image_toggle_method,
                                  base::Unretained(capture_mode_type_view)),
              kCaptureModeImageIcon));
  image_toggle_button->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_ASH_SCREEN_CAPTURE_TOOLTIP_SCREENSHOT));
  return image_toggle_button;
}

}  // namespace

CaptureModeTypeView::CaptureModeTypeView(bool projector_mode)
    : image_toggle_button_(
          MaybeCreateImageToggleButton(this,
                                       &CaptureModeTypeView::OnImageToggle,
                                       projector_mode)),
      video_toggle_button_(
          AddChildView(std::make_unique<CaptureModeToggleButton>(
              base::BindRepeating(&CaptureModeTypeView::OnVideoToggle,
                                  base::Unretained(this)),
              kCaptureModeVideoIcon))) {
  auto* color_provider = AshColorProvider::Get();
  const SkColor bg_color = color_provider->GetControlsLayerColor(
      AshColorProvider::ControlsLayerType::kControlBackgroundColorInactive);
  SetBackground(
      views::CreateRoundedRectBackground(bg_color, kBackgroundCornerRadius));
  SetBorder(views::CreateEmptyBorder(kViewInsets));
  auto* box_layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets(0),
      capture_mode::kSpaceBetweenCaptureModeTypeButtons));
  box_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  auto* controller = CaptureModeController::Get();

  if (controller->is_recording_in_progress()) {
    // We can't have more than one recording at the same time.
    video_toggle_button_->SetEnabled(false);
  }

  OnCaptureTypeChanged(controller->type());

  video_toggle_button_->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_ASH_SCREEN_CAPTURE_TOOLTIP_SCREENRECORD));
}

CaptureModeTypeView::~CaptureModeTypeView() = default;

void CaptureModeTypeView::OnCaptureTypeChanged(CaptureModeType new_type) {
  auto* controller = CaptureModeController::Get();
  const bool is_video = new_type == CaptureModeType::kVideo;

  DCHECK(!controller->is_recording_in_progress() || !is_video);

  video_toggle_button_->SetToggled(is_video);
  if (image_toggle_button_) {
    image_toggle_button_->SetToggled(!is_video);
    DCHECK_NE(image_toggle_button_->GetToggled(),
              video_toggle_button_->GetToggled());
  }

  auto* camera_controller = controller->camera_controller();
  // Set the value to true for `SetShouldShowPreview` when the capture mode
  // session is started and switched to a video recording mode before recording
  // starts. False when it is switched to image capture mode.
  // Don't trigger `SetShouldShowPreview` if there's a video recording in
  // progress, since the capture type is restricted to `kImage` at this use case
  // and we don't want to affect the camera preview for the in_progress video
  // recording.
  if (camera_controller && !controller->is_recording_in_progress()) {
    camera_controller->SetShouldShowPreview(is_video);
  }
}

void CaptureModeTypeView::OnImageToggle() {
  RecordCaptureModeBarButtonType(CaptureModeBarButtonType::kScreenCapture);
  CaptureModeController::Get()->SetType(CaptureModeType::kImage);
}

void CaptureModeTypeView::OnVideoToggle() {
  auto* controller = CaptureModeController::Get();
  DCHECK(!controller->is_recording_in_progress());
  RecordCaptureModeBarButtonType(CaptureModeBarButtonType::kScreenRecord);
  controller->SetType(CaptureModeType::kVideo);
}

BEGIN_METADATA(CaptureModeTypeView, views::View)
END_METADATA

}  // namespace ash
