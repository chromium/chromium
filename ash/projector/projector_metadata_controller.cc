// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/projector/projector_metadata_controller.h"

#include "ash/projector/projector_controller_impl.h"
#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/task/current_thread.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"

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

}  // namespace

ProjectorMetadataController::ProjectorMetadataController()
    : blocking_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})) {}

ProjectorMetadataController::~ProjectorMetadataController() = default;

void ProjectorMetadataController::OnRecordingStarted() {
  metadata_ = std::make_unique<ProjectorMetadata>();
}

void ProjectorMetadataController::RecordTranscription(
    const std::string& transcription,
    const base::TimeDelta start_time,
    const base::TimeDelta end_time,
    const std::vector<base::TimeDelta>& word_alignments) {
  DCHECK(metadata_);
  metadata_->AddTranscript(std::make_unique<ProjectorTranscript>(
      start_time, end_time, transcription, word_alignments));
}

void ProjectorMetadataController::RecordKeyIdea() {
  DCHECK(metadata_);
  metadata_->MarkKeyIdea();
}

void ProjectorMetadataController::SaveMetadata(
    const base::FilePath& video_file_path) {
  DCHECK(metadata_);
  // TODO(crbug.com/1165439): Finalize on the metadata file naming convention.
  const base::FilePath path = video_file_path.ReplaceExtension(".txt");

  metadata_->SetName(
      video_file_path.RemoveExtension().BaseName().AsUTF8Unsafe());

  // Save metadata.
  auto metadata_str = metadata_->Serialize();

  // TODO(crbug.com/1165439): Update after finalizing on the storage strategy.
  blocking_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&SaveFile, metadata_str, path),
      base::BindOnce(
          [](const base::FilePath& path, bool success) {
            if (!success) {
              LOG(ERROR) << "Failed to save the metadata file: " << path;
              return;
            }

            // TODO(crbug.com/1165439): Make screencast metadata indexable.
          },
          path));
}

void ProjectorMetadataController::SetProjectorMetadataModelForTest(
    std::unique_ptr<ProjectorMetadata> metadata) {
  metadata_ = std::move(metadata);
}

}  // namespace ash
