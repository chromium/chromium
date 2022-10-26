// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_type_view.h"

#include <memory>

#include "ash/capture_mode/capture_mode_camera_controller.h"
#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/capture_mode_metrics.h"
#include "ash/capture_mode/capture_mode_session_focus_cycler.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/icon_button.h"
#include "ash/style/icon_switch.h"
#include "base/bind.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {

CaptureModeTypeView::CaptureModeTypeView(bool projector_mode)
    : capture_type_switch_(AddChildView(std::make_unique<IconSwitch>())) {
  // If it's not a projector session, add image toggle button.
  if (!projector_mode) {
    image_toggle_button_ = capture_type_switch_->AddButton(
        base::BindRepeating(&CaptureModeTypeView::OnImageToggle,
                            base::Unretained(this)),
        &kCaptureModeImageIcon,
        l10n_util::GetStringUTF16(IDS_ASH_SCREEN_CAPTURE_TOOLTIP_SCREENSHOT));

    // Add highlight helper to image toggle button.
    CaptureModeSessionFocusCycler::HighlightHelper::Install(
        image_toggle_button_);
  }

  // Add video toggle button.
  video_toggle_button_ = capture_type_switch_->AddButton(
      base::BindRepeating(&CaptureModeTypeView::OnVideoToggle,
                          base::Unretained(this)),
      &kCaptureModeVideoIcon,
      l10n_util::GetStringUTF16(IDS_ASH_SCREEN_CAPTURE_TOOLTIP_SCREENRECORD));
  // Add highlight helper to video toggle button.
  CaptureModeSessionFocusCycler::HighlightHelper::Install(video_toggle_button_);

  auto* controller = CaptureModeController::Get();

  if (controller->is_recording_in_progress()) {
    // We can't have more than one recording at the same time.
    video_toggle_button_->SetEnabled(false);
  }

  SetLayoutManager(std::make_unique<views::FillLayout>());
}

CaptureModeTypeView::~CaptureModeTypeView() = default;

void CaptureModeTypeView::OnCaptureTypeChanged(CaptureModeType new_type) {
  auto* controller = CaptureModeController::Get();
  const bool is_video = new_type == CaptureModeType::kVideo;

  DCHECK(!controller->is_recording_in_progress() || !is_video);

  video_toggle_button_->SetToggled(is_video);

  if (image_toggle_button_)
    image_toggle_button_->SetToggled(!is_video);

  auto* camera_controller = controller->camera_controller();
  DCHECK(camera_controller);
  // Set the value to true for `SetShouldShowPreview` when the capture mode
  // session is started and switched to a video recording mode before recording
  // starts. False when it is switched to image capture mode.
  // Don't trigger `SetShouldShowPreview` if there's a video recording in
  // progress, since the capture type is restricted to `kImage` at this use case
  // and we don't want to affect the camera preview for the in_progress video
  // recording.
  if (!controller->is_recording_in_progress())
    camera_controller->SetShouldShowPreview(is_video);
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
