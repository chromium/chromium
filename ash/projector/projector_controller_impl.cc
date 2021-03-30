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
#include "ui/aura/window.h"

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
    base::TimeDelta audio_start_time,
    base::TimeDelta audio_end_time,
    const std::vector<base::TimeDelta>& word_offsets,
    bool is_final) {
  std::string transcript = base::UTF16ToUTF8(text);

  if (is_final) {
    // Records final transcript.
    metadata_controller_->RecordTranscription(transcript, audio_start_time,
                                              audio_end_time, word_offsets);
  }

  // Render transcription.
  if (is_caption_on_) {
    ui_controller_->OnTranscription(transcript, is_final);
  }
}

void ProjectorControllerImpl::SetProjectorToolsVisible(bool is_visible) {
  // TODO(yilkal): Projector toolbar shouldn't be shown if soda is not
  // available.
  if (is_visible)
    ui_controller_->ShowToolbar();
  else
    ui_controller_->CloseToolbar();
}

void ProjectorControllerImpl::StartProjectorSession(SourceType scope,
                                                    aura::Window* window) {
  // TODO(https://crbug.com/1185262): Start projector session.
}

bool ProjectorControllerImpl::IsEligible() const {
  return is_speech_recognition_available_;
}

void ProjectorControllerImpl::SetCaptionState(bool is_on) {
  if (is_on == is_caption_on_)
    return;

  is_caption_on_ = is_on;
}

void ProjectorControllerImpl::MarkKeyIdea() {
  metadata_controller_->RecordKeyIdea();
  ui_controller_->OnKeyIdeaMarked();
}

void ProjectorControllerImpl::OnRecordingStarted() {
  StartSpeechRecognition();
  metadata_controller_->OnRecordingStarted();
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
