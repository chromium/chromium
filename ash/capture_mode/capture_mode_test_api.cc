// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/capture_mode/capture_mode_test_api.h"

#include "ash/capture_mode/camera_video_frame_handler.h"
#include "ash/capture_mode/camera_video_frame_renderer.h"
#include "ash/capture_mode/capture_mode_camera_controller.h"
#include "ash/capture_mode/capture_mode_camera_preview_view.h"
#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/capture_mode_metrics.h"
#include "ash/capture_mode/capture_mode_session.h"
#include "ash/capture_mode/capture_mode_types.h"
#include "ash/capture_mode/video_recording_watcher.h"
#include "base/auto_reset.h"
#include "base/check.h"
#include "base/run_loop.h"

namespace ash {

namespace {

CaptureModeController* GetController() {
  auto* controller = CaptureModeController::Get();
  DCHECK(controller);
  return controller;
}

}  // namespace

CaptureModeTestApi::CaptureModeTestApi() : controller_(GetController()) {}

void CaptureModeTestApi::StartForFullscreen(bool for_video) {
  SetType(for_video);
  controller_->SetSource(CaptureModeSource::kFullscreen);
  controller_->Start(CaptureModeEntryType::kQuickSettings);
}

void CaptureModeTestApi::StartForWindow(bool for_video) {
  SetType(for_video);
  controller_->SetSource(CaptureModeSource::kWindow);
  controller_->Start(CaptureModeEntryType::kQuickSettings);
}

void CaptureModeTestApi::StartForRegion(bool for_video) {
  SetType(for_video);
  controller_->SetSource(CaptureModeSource::kRegion);
  controller_->Start(CaptureModeEntryType::kQuickSettings);
}

void CaptureModeTestApi::SetCaptureModeSource(CaptureModeSource source) {
  controller_->SetSource(source);
}

void CaptureModeTestApi::SetRecordingType(RecordingType recording_type) {
  controller_->SetRecordingType(recording_type);
}

bool CaptureModeTestApi::IsSessionActive() const {
  return controller_->IsActive();
}

void CaptureModeTestApi::SetUserSelectedRegion(const gfx::Rect& region) {
  controller_->SetUserCaptureRegion(region, /*by_user=*/true);
}

void CaptureModeTestApi::PerformCapture(bool skip_count_down) {
  DCHECK(controller_->IsActive());
  if (skip_count_down) {
    base::AutoReset<bool> skip_count_down_resetter(
        &controller_->skip_count_down_ui_, true);
    controller_->PerformCapture();
  } else {
    controller_->PerformCapture();
  }
}

bool CaptureModeTestApi::IsVideoRecordingInProgress() const {
  return controller_->is_recording_in_progress();
}

bool CaptureModeTestApi::IsPendingDlpCheck() const {
  return controller_->pending_dlp_check_;
}

bool CaptureModeTestApi::IsSessionWaitingForDlpConfirmation() const {
  return controller_->IsActive() &&
         controller_->capture_mode_session_->is_waiting_for_dlp_confirmation_;
}

bool CaptureModeTestApi::IsInCountDownAnimation() const {
  return controller_->IsActive() &&
         controller_->capture_mode_session_->IsInCountDownAnimation();
}

void CaptureModeTestApi::StopVideoRecording() {
  DCHECK(controller_->is_recording_in_progress());
  controller_->EndVideoRecording(EndRecordingReason::kStopRecordingButton);
}

void CaptureModeTestApi::SetOnCaptureFileSavedCallback(
    OnFileSavedCallback callback) {
  controller_->on_file_saved_callback_for_test_ = std::move(callback);
}

void CaptureModeTestApi::SetOnCaptureFileDeletedCallback(
    OnFileDeletedCallback callback) {
  controller_->on_file_deleted_callback_for_test_ = std::move(callback);
}

void CaptureModeTestApi::SetOnVideoRecordCountdownFinishedCallback(
    base::OnceClosure callback) {
  controller_->on_countdown_finished_callback_for_test_ = std::move(callback);
}

void CaptureModeTestApi::SetAudioRecordingEnabled(bool enabled) {
  DCHECK(!controller_->is_recording_in_progress());
  controller_->enable_audio_recording_ = enabled;
}

bool CaptureModeTestApi::GetAudioRecordingEnabled() const {
  return controller_->GetAudioRecordingEnabled();
}

void CaptureModeTestApi::FlushRecordingServiceForTesting() {
  DCHECK(controller_->recording_service_remote_.is_bound());
  controller_->recording_service_remote_.FlushForTesting();
}

void CaptureModeTestApi::ResetRecordingServiceRemote() {
  DCHECK(controller_->is_recording_in_progress());
  controller_->recording_service_remote_.reset();
}

void CaptureModeTestApi::ResetRecordingServiceClientReceiver() {
  DCHECK(controller_->is_recording_in_progress());
  controller_->recording_service_client_receiver_.reset();
}

RecordingOverlayController*
CaptureModeTestApi::GetRecordingOverlayController() {
  DCHECK(controller_->is_recording_in_progress());
  DCHECK(controller_->video_recording_watcher_->is_in_projector_mode());
  return controller_->video_recording_watcher_->recording_overlay_controller_
      .get();
}

void CaptureModeTestApi::SimulateOpeningFolderSelectionDialog() {
  DCHECK(controller_->IsActive());
  auto* session = controller_->capture_mode_session();
  DCHECK(!session->capture_mode_settings_widget_);
  session->SetSettingsMenuShown(true);
  DCHECK(session->capture_mode_settings_widget_);
  session->OpenFolderSelectionDialog();

  // In browser tests, the dialog creation is asynchronous, so we'll need to
  // wait for it.
  if (GetFolderSelectionDialogWindow())
    return;

  base::RunLoop loop;
  session->folder_selection_dialog_controller_
      ->on_dialog_window_added_callback_for_test_ = loop.QuitClosure();
  loop.Run();
}

aura::Window* CaptureModeTestApi::GetFolderSelectionDialogWindow() {
  DCHECK(controller_->IsActive());
  auto* session = controller_->capture_mode_session();
  auto* dialog_controller = session->folder_selection_dialog_controller_.get();
  return dialog_controller ? dialog_controller->dialog_window() : nullptr;
}

void CaptureModeTestApi::SetForceUseGpuMemoryBufferForCameraFrames(bool value) {
  DCHECK(controller_->camera_controller());
  CameraVideoFrameHandler::SetForceUseGpuMemoryBufferForTest(value);
}

size_t CaptureModeTestApi::GetNumberOfAvailableCameras() const {
  DCHECK(controller_->camera_controller());
  return controller_->camera_controller()->available_cameras().size();
}

void CaptureModeTestApi::SelectCameraAtIndex(size_t index) {
  auto* camera_controller = controller_->camera_controller();
  DCHECK(camera_controller);
  DCHECK_LT(index, GetNumberOfAvailableCameras());
  const auto& camera_info = camera_controller->available_cameras()[index];
  camera_controller->SetSelectedCamera(camera_info.camera_id);
}

void CaptureModeTestApi::TurnCameraOff() {
  auto* camera_controller = controller_->camera_controller();
  DCHECK(camera_controller);
  camera_controller->SetSelectedCamera(CameraId());
}

void CaptureModeTestApi::SetOnCameraVideoFrameRendered(
    CameraVideoFrameCallback callback) {
  auto* camera_controller = controller_->camera_controller();
  DCHECK(camera_controller);
  DCHECK(camera_controller->camera_preview_widget());
  DCHECK(camera_controller->camera_preview_view_);
  camera_controller->camera_preview_view_->camera_video_renderer_
      .on_video_frame_rendered_for_test_ = std::move(callback);
}

views::Widget* CaptureModeTestApi::GetCameraPreviewWidget() {
  return controller_->camera_controller()->camera_preview_widget();
}

void CaptureModeTestApi::SetType(bool for_video) {
  controller_->SetType(for_video ? CaptureModeType::kVideo
                                 : CaptureModeType::kImage);
}

}  // namespace ash
