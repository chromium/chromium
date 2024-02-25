// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_type_view.h"

#include <memory>

#include "ash/capture_mode/capture_mode_behavior.h"
#include "ash/capture_mode/capture_mode_camera_controller.h"
#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/capture_mode_metrics.h"
#include "ash/capture_mode/capture_mode_session.h"
#include "ash/capture_mode/capture_mode_session_focus_cycler.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/tab_slider.h"
#include "ash/style/tab_slider_button.h"
#include "base/functional/bind.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {

CaptureModeTypeView::CaptureModeTypeView(CaptureModeBehavior* active_behavior)
    : capture_type_switch_(AddChildView(std::make_unique<TabSlider>(
          /*max_tab_num=*/2,
          TabSlider::InitParams{/*internal_border_padding=*/2,
                                /*between_child_spacing=*/0,
                                /*has_background=*/true,
                                /*has_selector_animation=*/true,
                                /*distribute_space_evenly=*/false}))) {
  CHECK(active_behavior);

  // Only add the image toggle button if the active behavior allows.
  if (active_behavior->ShouldImageCaptureTypeBeAllowed()) {
    image_toggle_button_ = capture_type_switch_->AddButton<IconSliderButton>(
        base::BindRepeating(&CaptureModeTypeView::OnImageToggle,
                            base::Unretained(this)),
        &kCaptureModeImageIcon,
        l10n_util::GetStringUTF16(IDS_ASH_SCREEN_CAPTURE_TOOLTIP_SCREENSHOT));

    // Add highlight helper to image toggle button.
    CaptureModeSessionFocusCycler::HighlightHelper::Install(
        image_toggle_button_);
  }

  // Add video toggle button.
  video_toggle_button_ = capture_type_switch_->AddButton<IconSliderButton>(
      base::BindRepeating(&CaptureModeTypeView::OnVideoToggle,
                          base::Unretained(this)),
      &kCaptureModeVideoIcon,
      l10n_util::GetStringUTF16(IDS_ASH_SCREEN_CAPTURE_TOOLTIP_SCREENRECORD));
  // Add highlight helper to video toggle button.
  CaptureModeSessionFocusCycler::HighlightHelper::Install(video_toggle_button_);

  auto* controller = CaptureModeController::Get();

  if (!controller->can_start_new_recording()) {
    // We can't have more than one recording at the same time.
    video_toggle_button_->SetEnabled(false);
  }

  SetLayoutManager(std::make_unique<views::FillLayout>());
}

CaptureModeTypeView::~CaptureModeTypeView() = default;

void CaptureModeTypeView::OnCaptureTypeChanged(CaptureModeType new_type) {
  auto* controller = CaptureModeController::Get();
  const bool is_video = new_type == CaptureModeType::kVideo;

  DCHECK(controller->can_start_new_recording() || !is_video);

  video_toggle_button_->SetSelected(is_video);

  if (image_toggle_button_) {
    image_toggle_button_->SetSelected(!is_video);
  }
}

void CaptureModeTypeView::OnImageToggle() {
  RecordCaptureModeBarButtonType(CaptureModeBarButtonType::kScreenCapture);
  CaptureModeController::Get()->SetType(CaptureModeType::kImage);
}

void CaptureModeTypeView::OnVideoToggle() {
  auto* controller = CaptureModeController::Get();
  DCHECK(controller->can_start_new_recording());
  RecordCaptureModeBarButtonType(CaptureModeBarButtonType::kScreenRecord);
  controller->SetType(CaptureModeType::kVideo);
}

BEGIN_METADATA(CaptureModeTypeView)
END_METADATA

}  // namespace ash
