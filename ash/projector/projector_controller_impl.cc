// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/projector/projector_controller_impl.h"

#include "ash/projector/projector_metadata_controller.h"
#include "ash/projector/projector_ui_controller.h"
#include "ash/public/cpp/projector/projector_client.h"
#include "ash/shell.h"

namespace ash {

ProjectorControllerImpl::ProjectorControllerImpl()
    : ui_controller_(std::make_unique<ash::ProjectorUiController>()),
      metadata_controller_(
          std::make_unique<ash::ProjectorMetadataController>()) {}

ProjectorControllerImpl::~ProjectorControllerImpl() = default;

void ProjectorControllerImpl::SetClient(ProjectorClient* client) {
  client_ = client;
}

void ProjectorControllerImpl::ShowToolbar() {
  ui_controller_->ShowToolbar();
}

void ProjectorControllerImpl::SetCaptionState(bool is_on) {
  if (is_on == is_caption_on_)
    return;

  is_caption_on_ = is_on;
}

void ProjectorControllerImpl::OnRecordingStarted() {
  StartSpeechRecognition();
  metadata_controller_->OnRecordingStarted();
}

void ProjectorControllerImpl::SaveScreencast(
    const base::FilePath& saved_video_path) {
  metadata_controller_->SaveMetadata(saved_video_path);
}

void ProjectorControllerImpl::OnTranscription(
    chromeos::machine_learning::mojom::SpeechRecognizerEventPtr
        speech_recognizer_event) {
  bool is_final = speech_recognizer_event->is_final_result();
  std::string transcript;

  if (is_final) {
    auto& final_result = speech_recognizer_event->get_final_result();

    if (final_result->final_hypotheses.size() > 0) {
      // Get the first result which is the most likely.
      transcript = final_result->final_hypotheses.at(0);
    }

    // Records final transcript.
    metadata_controller_->RecordTranscription(
        transcript, final_result->timing_event->audio_start_time,
        final_result->timing_event->event_end_time,
        final_result->timing_event->word_alignments);
  } else if (speech_recognizer_event->is_partial_result()) {
    auto& partial_text =
        speech_recognizer_event->get_partial_result()->partial_text;
    if (partial_text.size() > 0) {
      // Get the first result which is the most likely.
      transcript = partial_text.at(0);
    }
  } else {
    LOG(ERROR) << "No valid speech recognition result.";
    return;
  }

  // Render transcription.
  if (is_caption_on_) {
    ui_controller_->OnTranscription(transcript, is_final);
  }
}

void ProjectorControllerImpl::SetProjectorUiControllerForTest(
    std::unique_ptr<ProjectorUiController> ui_controller) {
  ui_controller_ = std::move(ui_controller);
}

void ProjectorControllerImpl::SetProjectorMetadataControllerForTest(
    std::unique_ptr<ProjectorMetadataController> metadata_controller) {
  metadata_controller_ = std::move(metadata_controller);
}

void ProjectorControllerImpl::MarkKeyIdea() {
  metadata_controller_->RecordKeyIdea();
  ui_controller_->OnKeyIdeaMarked();
}

void ProjectorControllerImpl::StartSpeechRecognition() {
  DCHECK(!is_speech_recognition_on_);
  DCHECK_NE(client_, nullptr);
  client_->StartSpeechRecognition();
  is_speech_recognition_on_ = true;
}

void ProjectorControllerImpl::StopSpeechRecognition() {
  DCHECK(is_speech_recognition_on_);
  DCHECK_NE(client_, nullptr);
  client_->StopSpeechRecognition();
  is_speech_recognition_on_ = false;
}

}  // namespace ash
