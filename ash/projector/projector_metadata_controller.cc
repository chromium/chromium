// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/projector/projector_metadata_controller.h"

#include "ash/projector/projector_metadata_model.h"
#include "ash/projector/projector_metrics.h"
#include "ash/projector/projector_ui_controller.h"
#include "ash/webui/projector_app/public/cpp/projector_app_constants.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/task/current_thread.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "third_party/icu/source/common/unicode/locid.h"

namespace ash {
namespace {

// Writes the given |data| in a file with |path|. Returns true if saving
// succeeded, or false otherwise.
bool SaveFile(const std::string& content, const base::FilePath& path) {
  DCHECK(!base::CurrentUIThread::IsSet());
  DCHECK(!path.empty());

  if (!base::PathExists(path.DirName()) &&
      !base::CreateDirectory(path.DirName())) {
    LOG(ERROR) << "Failed to create path: " << path.DirName();
    return false;
  }

  return base::WriteFile(path, content);
}

const std::string GetFormattedLangauge(const icu::Locale& locale) {
  const std::string language = locale.getLanguage();
  if (language == "zh") {
    return "zh_TW";
  }
  return language;
}

}  // namespace

ProjectorMetadataController::ProjectorMetadataController() = default;

ProjectorMetadataController::~ProjectorMetadataController() = default;

void ProjectorMetadataController::OnRecordingStarted() {
  metadata_ = std::make_unique<ProjectorMetadata>();
  metadata_->SetCaptionLanguage(
      GetFormattedLangauge(icu::Locale::getDefault()));
  metadata_->SetMetadataVersionNumber(MetadataVersionNumber::kV2);
}

void ProjectorMetadataController::RecordTranscription(
    const media::SpeechRecognitionResult& speech_result) {
  DCHECK(metadata_);

  const auto& timing = speech_result.timing_information;
  metadata_->AddTranscript(std::make_unique<ProjectorTranscript>(
      timing->audio_start_time, timing->audio_end_time,
      /*group_id=*/timing->audio_start_time.InMilliseconds(),
      speech_result.transcription, timing->hypothesis_parts.value()));
}

void ProjectorMetadataController::SetSpeechRecognitionStatus(
    RecognitionStatus status) {
  DCHECK(metadata_);
  metadata_->SetSpeechRecognitionStatus(status);
}

void ProjectorMetadataController::RecordKeyIdea() {
  DCHECK(metadata_);
  metadata_->MarkKeyIdea();
}

void ProjectorMetadataController::SaveMetadata(
    const base::FilePath& video_file_path) {
  DCHECK(metadata_);
  const base::FilePath path =
      video_file_path.AddExtension(kProjectorV2MetadataFileExtension);

  // Save metadata.
  auto metadata_str = metadata_->Serialize();

  // TODO(b/203000496): Update after finalizing on the storage strategy.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&SaveFile, metadata_str, path),
      base::BindOnce(&ProjectorMetadataController::OnSaveFileResult,
                     weak_factory_.GetWeakPtr(), path,
                     metadata_->GetTranscriptsCount()));
}

void ProjectorMetadataController::OnSaveFileResult(const base::FilePath& path,
                                                   size_t transcripts_count,
                                                   bool success) {
  if (!success) {
    LOG(ERROR) << "Failed to save the metadata file: " << path;
    ProjectorUiController::ShowSaveFailureNotification();
    return;
  }
  RecordTranscriptsCount(transcripts_count);
}

void ProjectorMetadataController::SetProjectorMetadataModelForTest(
    std::unique_ptr<ProjectorMetadata> metadata) {
  metadata_ = std::move(metadata);
}

}  // namespace ash
