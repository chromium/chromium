// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/capture_mode_test_api.h"

#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/capture_mode_metrics.h"
#include "ash/capture_mode/capture_mode_types.h"
#include "base/auto_reset.h"
#include "base/check.h"

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

void CaptureModeTestApi::SetUserSelectedRegion(const gfx::Rect& region) {
  controller_->SetUserCaptureRegion(region, /*by_user=*/true);
}

void CaptureModeTestApi::PerformCapture() {
  DCHECK(controller_->IsActive());
  base::AutoReset<bool> skip_count_down(&controller_->skip_count_down_ui_,
                                        true);
  controller_->PerformCapture();
}

void CaptureModeTestApi::StopVideoRecording() {
  DCHECK(controller_->is_recording_in_progress());
  controller_->EndVideoRecording(EndRecordingReason::kStopRecordingButton);
}

void CaptureModeTestApi::SetOnCaptureFileSavedCallback(
    OnFileSavedCallback callback) {
  controller_->on_file_saved_callback_ = std::move(callback);
}

void CaptureModeTestApi::SetAudioRecordingEnabled(bool enabled) {
  DCHECK(!controller_->is_recording_in_progress());
  controller_->enable_audio_recording_ = enabled;
}

void CaptureModeTestApi::FlushRecordingServiceForTesting() {
  DCHECK(controller_->is_recording_in_progress());
  controller_->recording_service_remote_.FlushForTesting();
}

void CaptureModeTestApi::SetType(bool for_video) {
  controller_->SetType(for_video ? CaptureModeType::kVideo
                                 : CaptureModeType::kImage);
}

}  // namespace ash
