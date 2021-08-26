// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/projector/projector_controller_impl.h"

#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/capture_mode_metrics.h"
#include "ash/projector/projector_metadata_controller.h"
#include "ash/projector/projector_ui_controller.h"
#include "ash/public/cpp/projector/projector_client.h"
#include "ash/public/cpp/projector/projector_session.h"
#include "ash/shell.h"
#include "base/strings/utf_string_conversions.h"
#include "media/mojo/mojom/speech_recognition_service.mojom.h"

namespace ash {

ProjectorControllerImpl::ProjectorControllerImpl()
    : projector_session_(std::make_unique<ash::ProjectorSessionImpl>()),
      ui_controller_(std::make_unique<ash::ProjectorUiController>(this)),
      metadata_controller_(
          std::make_unique<ash::ProjectorMetadataController>()) {}

ProjectorControllerImpl::~ProjectorControllerImpl() = default;

// static
ProjectorControllerImpl* ProjectorControllerImpl::Get() {
  return static_cast<ProjectorControllerImpl*>(ProjectorController::Get());
}

void ProjectorControllerImpl::StartProjectorSession(
    const std::string& storage_dir) {
  DCHECK(CanStartNewSession());

  auto* controller = CaptureModeController::Get();
  if (!controller->is_recording_in_progress()) {
    projector_session_->Start(storage_dir);
    controller->Start(CaptureModeEntryType::kProjector);
  }
}

void ProjectorControllerImpl::SetClient(ProjectorClient* client) {
  client_ = client;
}

void ProjectorControllerImpl::OnSpeechRecognitionAvailable(bool available) {
  if (available == is_speech_recognition_available_)
    return;

  is_speech_recognition_available_ = available;
}

void ProjectorControllerImpl::OnTranscription(
    const media::SpeechRecognitionResult& result) {
  // Render transcription.
  if (is_caption_on_) {
    ui_controller_->OnTranscription(result.transcription, result.is_final);
  }

  if (result.is_final && result.timing_information.has_value()) {
    // Records final transcript.
    metadata_controller_->RecordTranscription(result);
  }
}

void ProjectorControllerImpl::OnTranscriptionError() {
  CaptureModeController::Get()->EndVideoRecording(
      EndRecordingReason::kProjectorTranscriptionError);
}

bool ProjectorControllerImpl::IsEligible() const {
  return is_speech_recognition_available_;
}

bool ProjectorControllerImpl::CanStartNewSession() const {
  // TODO(crbug.com/1165435) Add other pre-conditions to starting a new
  // projector session.
  return IsEligible() && !projector_session_->is_active();
}

void ProjectorControllerImpl::SetCaptionBubbleState(bool is_on) {
  ui_controller_->SetCaptionBubbleState(is_on);
}

void ProjectorControllerImpl::OnCaptionBubbleModelStateChanged(bool is_on) {
  is_caption_on_ = is_on;
}

void ProjectorControllerImpl::MarkKeyIdea() {
  metadata_controller_->RecordKeyIdea();
  ui_controller_->OnKeyIdeaMarked();
}

void ProjectorControllerImpl::OnRecordingStarted() {
  ui_controller_->ShowToolbar();
  StartSpeechRecognition();
  ui_controller_->OnRecordingStateChanged(true /* started */);
  metadata_controller_->OnRecordingStarted();
}

void ProjectorControllerImpl::OnRecordingEnded() {
  DCHECK(projector_session_->is_active());

  StopSpeechRecognition();
  ui_controller_->OnRecordingStateChanged(false /* started */);

  // TODO(b/197152209): move closing selfie cam to ProjectorUiController.
  if (client_->IsSelfieCamVisible())
    client_->CloseSelfieCam();
  // Close Projector toolbar.
  ui_controller_->CloseToolbar();

  // TODO(crbug.com/1165439): Call on to SaveScreencast when the storage
  // strategy is finalized.
}

void ProjectorControllerImpl::SaveScreencast(
    const base::FilePath& saved_video_path) {
  metadata_controller_->SaveMetadata(saved_video_path);

  // TODO(crbug.com/1165439): Stop projector session when the screencast is
  // saved.
  projector_session_->Stop();
}

void ProjectorControllerImpl::OnLaserPointerPressed() {
  ui_controller_->OnLaserPointerPressed();
}

void ProjectorControllerImpl::OnMarkerPressed() {
  ui_controller_->OnMarkerPressed();
}

void ProjectorControllerImpl::OnClearAllMarkersPressed() {
  ui_controller_->OnClearAllMarkersPressed();
}

void ProjectorControllerImpl::OnUndoPressed() {
  ui_controller_->OnUndoPressed();
}

void ProjectorControllerImpl::OnSelfieCamPressed(bool enabled) {
  ui_controller_->OnSelfieCamPressed(enabled);

  DCHECK_NE(client_, nullptr);
  if (enabled == client_->IsSelfieCamVisible())
    return;

  if (enabled) {
    client_->ShowSelfieCam();
    return;
  }
  client_->CloseSelfieCam();
}

void ProjectorControllerImpl::OnMagnifierButtonPressed(bool enabled) {
  ui_controller_->OnMagnifierButtonPressed(enabled);
}

void ProjectorControllerImpl::OnChangeMarkerColorPressed(SkColor new_color) {
  ui_controller_->OnChangeMarkerColorPressed(new_color);
}

void ProjectorControllerImpl::SetProjectorUiControllerForTest(
    std::unique_ptr<ProjectorUiController> ui_controller) {
  ui_controller_ = std::move(ui_controller);
}

void ProjectorControllerImpl::SetProjectorMetadataControllerForTest(
    std::unique_ptr<ProjectorMetadataController> metadata_controller) {
  metadata_controller_ = std::move(metadata_controller);
}

void ProjectorControllerImpl::StartSpeechRecognition() {
  DCHECK(is_speech_recognition_available_);
  DCHECK(!is_speech_recognition_on_);
  DCHECK_NE(client_, nullptr);
  client_->StartSpeechRecognition();
  is_speech_recognition_on_ = true;
}

void ProjectorControllerImpl::StopSpeechRecognition() {
  DCHECK(is_speech_recognition_available_);
  DCHECK(is_speech_recognition_on_);
  DCHECK_NE(client_, nullptr);
  client_->StopSpeechRecognition();
  is_speech_recognition_on_ = false;
}

}  // namespace ash
