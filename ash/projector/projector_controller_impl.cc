// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/projector/projector_controller_impl.h"

#include "ash/projector/projector_metadata_controller.h"
#include "ash/projector/projector_ui_controller.h"
#include "ash/public/cpp/projector/projector_client.h"
#include "ash/public/cpp/projector/projector_session.h"
#include "ash/shell.h"
#include "base/strings/utf_string_conversions.h"

namespace ash {

ProjectorControllerImpl::ProjectorControllerImpl()
    : projector_session_(std::make_unique<ash::ProjectorSessionImpl>()),
      ui_controller_(std::make_unique<ash::ProjectorUiController>(this)),
      metadata_controller_(
          std::make_unique<ash::ProjectorMetadataController>()) {}

ProjectorControllerImpl::~ProjectorControllerImpl() = default;

void ProjectorControllerImpl::SetClient(ProjectorClient* client) {
  client_ = client;
}

void ProjectorControllerImpl::OnSpeechRecognitionAvailable(bool available) {
  if (available == is_speech_recognition_available_)
    return;

  is_speech_recognition_available_ = available;
}

void ProjectorControllerImpl::OnTranscription(
    const std::u16string& text,
    absl::optional<base::TimeDelta> start_time,
    absl::optional<base::TimeDelta> end_time,
    const absl::optional<std::vector<base::TimeDelta>>& word_offsets,
    bool is_final) {
  std::string transcript = base::UTF16ToUTF8(text);

  if (is_final && start_time.has_value() && end_time.has_value() &&
      word_offsets.has_value()) {
    // Records final transcript.
    metadata_controller_->RecordTranscription(
        transcript, start_time.value(), end_time.value(), word_offsets.value());
  }

  // Render transcription.
  if (is_caption_on_) {
    ui_controller_->OnTranscription(transcript, is_final);
  }
}

void ProjectorControllerImpl::SetProjectorToolsVisible(bool is_visible) {
  // TODO(yilkal): Projector toolbar shouldn't be shown if soda is not
  // available.
  if (is_visible) {
    ui_controller_->ShowToolbar();
    OnRecordingStarted();
    return;
  }

  OnRecordingEnded();
  if (client_->IsSelfieCamVisible())
    client_->CloseSelfieCam();
  ui_controller_->CloseToolbar();
}

bool ProjectorControllerImpl::IsEligible() const {
  return is_speech_recognition_available_;
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
  StartSpeechRecognition();
  ui_controller_->OnRecordingStateChanged(true /* started */);
  metadata_controller_->OnRecordingStarted();
}

void ProjectorControllerImpl::OnRecordingEnded() {
  StopSpeechRecognition();
  ui_controller_->OnRecordingStateChanged(false /* started */);

  // TODO(crbug.com/1165439): Call on to SaveScreencast when the metadata file
  // saving format is finalized.
}

void ProjectorControllerImpl::SaveScreencast(
    const base::FilePath& saved_video_path) {
  metadata_controller_->SaveMetadata(saved_video_path);
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
