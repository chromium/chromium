// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/io_task.h"

#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "storage/browser/file_system/file_system_url.h"

namespace file_manager::io_task {

void IOTask::Resume(ResumeParams) {}

EntryStatus::EntryStatus(storage::FileSystemURL file_url,
                         absl::optional<base::File::Error> file_error)
    : url(file_url), error(file_error) {}
EntryStatus::~EntryStatus() = default;

EntryStatus::EntryStatus(EntryStatus&& other) = default;
EntryStatus& EntryStatus::operator=(EntryStatus&& other) = default;

ProgressStatus::ProgressStatus() = default;
ProgressStatus::~ProgressStatus() = default;

ProgressStatus::ProgressStatus(ProgressStatus&& other) = default;
ProgressStatus& ProgressStatus::operator=(ProgressStatus&& other) = default;

bool ProgressStatus::IsPaused() const {
  return state == State::kPaused;
}

bool ProgressStatus::IsCompleted() const {
  return state == State::kSuccess || state == State::kError ||
         state == State::kCancelled;
}

std::string ProgressStatus::GetSourceName(Profile* profile) const {
  if (!source_name.empty()) {
    return source_name;
  }

  if (sources.size() == 0) {
    return {};
  }

  return util::GetDisplayablePath(profile, sources.front().url)
      .value_or(base::FilePath())
      .BaseName()
      .value();
}

void ProgressStatus::SetDestinationFolder(storage::FileSystemURL folder,
                                          Profile* const profile) {
  destination_folder_ = std::move(folder);
  destination_volume_id_.clear();
  if (profile) {
    if (VolumeManager* const volume_manager = VolumeManager::Get(profile)) {
      if (const base::WeakPtr<const Volume> volume =
              volume_manager->FindVolumeFromPath(destination_folder_.path())) {
        destination_volume_id_ = volume->volume_id();
      }
    }
  }
}

DummyIOTask::DummyIOTask(std::vector<storage::FileSystemURL> source_urls,
                         storage::FileSystemURL destination_folder,
                         OperationType type,
                         bool show_notifications)
    : IOTask(show_notifications) {
  progress_.state = State::kQueued;
  progress_.type = type;
  progress_.SetDestinationFolder(std::move(destination_folder));
  progress_.bytes_transferred = 0;
  progress_.total_bytes = 2;

  for (auto& url : source_urls) {
    progress_.sources.emplace_back(url, absl::nullopt);
  }
}

DummyIOTask::~DummyIOTask() = default;

void DummyIOTask::Execute(IOTask::ProgressCallback progress_callback,
                          IOTask::CompleteCallback complete_callback) {
  progress_callback_ = std::move(progress_callback);
  complete_callback_ = std::move(complete_callback);

  progress_.state = State::kInProgress;
  progress_callback_.Run(progress_);

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&DummyIOTask::DoProgress, weak_ptr_factory_.GetWeakPtr()));
}

void DummyIOTask::DoProgress() {
  progress_.bytes_transferred = 1;
  progress_callback_.Run(progress_);

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&DummyIOTask::DoComplete, weak_ptr_factory_.GetWeakPtr()));
}

void DummyIOTask::DoComplete() {
  progress_.state = State::kSuccess;
  progress_.bytes_transferred = 2;
  for (auto& source : progress_.sources) {
    source.error.emplace(base::File::FILE_OK);
  }
  std::move(complete_callback_).Run(std::move(progress_));
}

void DummyIOTask::Cancel() {
  progress_.state = State::kCancelled;
}

}  // namespace file_manager::io_task
