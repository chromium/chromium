// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/null_capture_mode_session.h"
#include "base/notreached.h"
#include "ui/compositor/layer.h"

namespace ash {

NullCaptureModeSession::NullCaptureModeSession(
    CaptureModeController* controller,
    CaptureModeBehavior* active_behavior)
    : BaseCaptureModeSession(controller, active_behavior, SessionType::kReal) {}

views::Widget* NullCaptureModeSession::GetCaptureModeBarWidget() {
  // The null session will never have a bar widget, so this function should
  // never be called.
  NOTREACHED_NORETURN();
}

aura::Window* NullCaptureModeSession::GetSelectedWindow() const {
  CHECK_LE(selected_window_tracker_.windows().size(), 1u);
  return selected_window_tracker_.windows().size() == 1
             ? selected_window_tracker_.windows().front()
             : nullptr;
}

void NullCaptureModeSession::SetPreSelectedWindow(
    aura::Window* pre_selected_window) {
  selected_window_tracker_.Add(pre_selected_window);
}

void NullCaptureModeSession::OnCaptureSourceChanged(
    CaptureModeSource new_source) {
  // Currently, the null session only applies to game dashboard, which only
  // records selected windows.
  if (new_source != CaptureModeSource::kWindow) {
    NOTREACHED();
  }
}

void NullCaptureModeSession::OnCaptureTypeChanged(CaptureModeType new_type) {
  MaybeUpdateSelfieCamInSessionVisibility();
}

void NullCaptureModeSession::OnRecordingTypeChanged() {}

void NullCaptureModeSession::OnAudioRecordingModeChanged() {
  active_behavior_->OnAudioRecordingModeChanged();
}

void NullCaptureModeSession::OnDemoToolsSettingsChanged() {
  active_behavior_->OnDemoToolsSettingsChanged();
}

void NullCaptureModeSession::OnWaitingForDlpConfirmationStarted() {}

void NullCaptureModeSession::OnWaitingForDlpConfirmationEnded(bool reshow_uis) {
}

void NullCaptureModeSession::ReportSessionHistograms() {
  RecordCaptureModeConfiguration(
      controller_->type(), controller_->source(), controller_->recording_type(),
      controller_->GetEffectiveAudioRecordingMode(), active_behavior_);
}

void NullCaptureModeSession::StartCountDown(
    base::OnceClosure countdown_finished_callback) {
  // Skip the countdown as we are aiming to record immediately.
  std::move(countdown_finished_callback).Run();
}

void NullCaptureModeSession::OnCaptureFolderMayHaveChanged() {
  NOTREACHED();
}

void NullCaptureModeSession::OnDefaultCaptureFolderSelectionChanged() {
  NOTREACHED();
}

bool NullCaptureModeSession::CalculateCameraPreviewTargetVisibility() const {
  return true;
}

void NullCaptureModeSession::OnCameraPreviewDragStarted() {}

void NullCaptureModeSession::OnCameraPreviewDragEnded(
    const gfx::Point& screen_location,
    bool is_touch) {}

void NullCaptureModeSession::OnCameraPreviewBoundsOrVisibilityChanged(
    bool capture_surface_became_too_small,
    bool did_bounds_or_visibility_change) {}

void NullCaptureModeSession::OnCameraPreviewDestroyed() {}

void NullCaptureModeSession::MaybeDismissUserNudgeForever() {}

void NullCaptureModeSession::MaybeChangeRoot(aura::Window* new_root) {
  DCHECK(new_root->IsRootWindow());
  current_root_ = new_root;
}

std::set<aura::Window*>
NullCaptureModeSession::GetWindowsToIgnoreFromWidgets() {
  return std::set<aura::Window*>();
}

void NullCaptureModeSession::InitInternal() {
  layer()->SetName("NullCaptureModeSession");
}

void NullCaptureModeSession::ShutdownInternal() {}

}  // namespace ash
