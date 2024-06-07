// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/io_task.h"

#include <type_traits>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "storage/browser/file_system/file_system_url.h"

namespace file_manager::io_task {

std::ostream& operator<<(std::ostream& out, const State state) {
  switch (state) {
#define PRINT(s)    \
  case State::k##s: \
    return out << #s;
    PRINT(Queued)
    PRINT(Scanning)
    PRINT(InProgress)
    PRINT(Paused)
    PRINT(Success)
    PRINT(Error)
    PRINT(NeedPassword)
    PRINT(Cancelled)
#undef PRINT
  }

  return out << "State(" << static_cast<std::underlying_type_t<State>>(state)
             << ")";
}

std::ostream& operator<<(std::ostream& out, OperationType op) {
  switch (op) {
#define PRINT(s)            \
  case OperationType::k##s: \
    return out << #s;
    PRINT(Copy)
    PRINT(Delete)
    PRINT(EmptyTrash)
    PRINT(Extract)
    PRINT(Move)
    PRINT(Restore)
    PRINT(RestoreToDestination)
    PRINT(Trash)
    PRINT(Zip)
#undef PRINT
  }

  return out << "OperationType("
             << static_cast<std::underlying_type_t<OperationType>>(op) << ")";
}

void IOTask::Pause(PauseParams params) {}

void IOTask::Resume(ResumeParams) {}

void IOTask::CompleteWithError(PolicyError policy_error) {}

bool PolicyError::operator==(const PolicyError& other) const = default;

bool PolicyError::operator!=(const PolicyError& other) const = default;

bool ConflictPauseParams::operator==(const ConflictPauseParams& other) const =
    default;

bool PolicyPauseParams::operator==(const PolicyPauseParams& other) const =
    default;

PauseParams::PauseParams() = default;

PauseParams::PauseParams(const PauseParams& other) = default;

PauseParams& PauseParams::operator=(const PauseParams& other) = default;

PauseParams::PauseParams(PauseParams&& other) = default;

PauseParams& PauseParams::operator=(PauseParams&& other) = default;

bool PauseParams::operator==(const PauseParams& other) const {
  return (conflict_params == other.conflict_params) &&
         (policy_params == other.policy_params);
}

PauseParams::~PauseParams() = default;

ResumeParams::ResumeParams() = default;

ResumeParams::ResumeParams(const ResumeParams& other) = default;

ResumeParams& ResumeParams::operator=(const ResumeParams& other) = default;

ResumeParams::ResumeParams(ResumeParams&& other) = default;

ResumeParams& ResumeParams::operator=(ResumeParams&& other) = default;

ResumeParams::~ResumeParams() = default;

EntryStatus::EntryStatus(storage::FileSystemURL file_url,
                         std::optional<base::File::Error> file_error,
                         std::optional<storage::FileSystemURL> source_url)
    : url(file_url), error(file_error), source_url(source_url) {}

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

bool ProgressStatus::HasWarning() const {
  // We should show a warning if the task is paused because of policy.
  return IsPaused() && pause_params.policy_params.has_value();
}

bool ProgressStatus::HasPolicyError() const {
  return state == State::kError && policy_error.has_value();
}

bool ProgressStatus::IsScanning() const {
  return state == State::kScanning;
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
                         bool show_notifications,
                         bool progress_succeeds)
    : IOTask(show_notifications), progress_succeeds_(progress_succeeds) {
  progress_.state = State::kQueued;
  progress_.type = type;
  progress_.SetDestinationFolder(std::move(destination_folder));
  progress_.bytes_transferred = 0;
  progress_.total_bytes = 2;

  for (auto& url : source_urls) {
    progress_.sources.emplace_back(url, std::nullopt);
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

void DummyIOTask::Pause(PauseParams pause_params) {
  progress_.state = State::kPaused;
  progress_.pause_params = pause_params;
}

void DummyIOTask::Resume(ResumeParams resume_params) {
  progress_.state = State::kInProgress;
}

void DummyIOTask::Cancel() {
  progress_.state = State::kCancelled;
}

void DummyIOTask::CompleteWithError(PolicyError policy_error) {
  progress_.state = State::kError;
  progress_.policy_error.emplace(std::move(policy_error));
}

void DummyIOTask::DoProgress() {
  if (progress_.IsPaused()) {
    return;
  }

  progress_.bytes_transferred = 1;
  progress_callback_.Run(progress_);

  if (progress_succeeds_) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&DummyIOTask::DoComplete,
                                  weak_ptr_factory_.GetWeakPtr()));
  }
}

void DummyIOTask::DoComplete() {
  progress_.state = State::kSuccess;
  progress_.bytes_transferred = 2;
  for (auto& source : progress_.sources) {
    source.error.emplace(base::File::FILE_OK);
  }
  std::move(complete_callback_).Run(std::move(progress_));
}

}  // namespace file_manager::io_task
