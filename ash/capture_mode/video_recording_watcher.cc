// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/video_recording_watcher.h"

#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/capture_mode_metrics.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/notreached.h"
#include "ui/aura/window.h"
#include "ui/display/screen.h"

namespace ash {

VideoRecordingWatcher::VideoRecordingWatcher(
    CaptureModeController* controller,
    aura::Window* window_being_recorded)
    : controller_(controller),
      window_being_recorded_(window_being_recorded),
      recording_source_(controller_->source()) {
  DCHECK(controller_);
  DCHECK(window_being_recorded_);
  DCHECK(controller_->is_recording_in_progress());

  if (!window_being_recorded_->IsRootWindow()) {
    DCHECK_EQ(recording_source_, CaptureModeSource::kWindow);
    non_root_window_capture_request_ =
        window_being_recorded_->MakeWindowCapturable();
  }

  window_being_recorded_->AddObserver(this);
  display::Screen::GetScreen()->AddObserver(this);
}

VideoRecordingWatcher::~VideoRecordingWatcher() {
  DCHECK(window_being_recorded_);

  display::Screen::GetScreen()->RemoveObserver(this);
  window_being_recorded_->RemoveObserver(this);
}

void VideoRecordingWatcher::OnWindowDestroying(aura::Window* window) {
  DCHECK_EQ(window, window_being_recorded_);
  DCHECK(controller_->is_recording_in_progress());

  // EndVideoRecording() destroys |this|. No need to remove observer here, since
  // it will be done in the destructor.
  controller_->EndVideoRecording(EndRecordingReason::kDisplayOrWindowClosing);
}

void VideoRecordingWatcher::OnWindowDestroyed(aura::Window* window) {
  DCHECK_EQ(window, window_being_recorded_);

  // We should never get here, since OnWindowDestroying() calls
  // EndVideoRecording() which deletes us.
  NOTREACHED();
}

void VideoRecordingWatcher::OnWindowRemovingFromRootWindow(
    aura::Window* window,
    aura::Window* new_root) {
  DCHECK_EQ(window, window_being_recorded_);
  DCHECK(controller_->is_recording_in_progress());
  DCHECK_EQ(recording_source_, CaptureModeSource::kWindow);

  if (!new_root)
    return;

  controller_->OnRecordedWindowChangingRoot(window_being_recorded_, new_root);
}

void VideoRecordingWatcher::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t metrics) {
  if (recording_source_ == CaptureModeSource::kFullscreen)
    return;

  if (!(metrics & (DISPLAY_METRIC_BOUNDS | DISPLAY_METRIC_ROTATION |
                   DISPLAY_METRIC_DEVICE_SCALE_FACTOR))) {
    return;
  }

  auto* root = window_being_recorded_->GetRootWindow();
  const int64_t display_id =
      display::Screen::GetScreen()->GetDisplayNearestWindow(root).id();
  if (display_id != display.id())
    return;

  controller_->PushNewRootSizeToRecordingService(root->bounds().size());
}

}  // namespace ash
