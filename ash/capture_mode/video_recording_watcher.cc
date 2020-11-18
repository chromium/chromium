// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/video_recording_watcher.h"

#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/capture_mode_util.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/notreached.h"
#include "ui/aura/window.h"

namespace ash {

VideoRecordingWatcher::VideoRecordingWatcher(
    CaptureModeController* controller,
    aura::Window* window_being_recorded)
    : controller_(controller), window_being_recorded_(window_being_recorded) {
  DCHECK(controller_);
  DCHECK(window_being_recorded_);
  DCHECK(controller_->is_recording_in_progress());

  window_being_recorded_->AddObserver(this);
  Shell::Get()->session_controller()->AddObserver(this);
}

VideoRecordingWatcher::~VideoRecordingWatcher() {
  DCHECK(window_being_recorded_);

  window_being_recorded_->RemoveObserver(this);
  Shell::Get()->session_controller()->RemoveObserver(this);
}

void VideoRecordingWatcher::OnWindowDestroying(aura::Window* window) {
  DCHECK_EQ(window, window_being_recorded_);
  DCHECK(controller_->is_recording_in_progress());

  // EndVideoRecording() destroys |this|. No need to remove observer here, since
  // it will be done in the destructor.
  controller_->EndVideoRecording();
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

  if (!new_root)
    return;

  // When a window being recorded changes displays either due to a display
  // getting disconnected, or moved by the user, the stop-recording button
  // should follow that window to that display.
  capture_mode_util::SetStopRecordingButtonVisibility(window->GetRootWindow(),
                                                      false);
  capture_mode_util::SetStopRecordingButtonVisibility(new_root, true);
}

void VideoRecordingWatcher::OnSessionStateChanged(
    session_manager::SessionState state) {
  DCHECK(controller_->is_recording_in_progress());

  if (Shell::Get()->session_controller()->IsUserSessionBlocked())
    controller_->EndVideoRecording();
}

void VideoRecordingWatcher::OnChromeTerminating() {
  DCHECK(controller_->is_recording_in_progress());

  controller_->EndVideoRecording();
}

}  // namespace ash
