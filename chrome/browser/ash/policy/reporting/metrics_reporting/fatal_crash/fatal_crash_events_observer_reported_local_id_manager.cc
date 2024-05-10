// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/fatal_crash/fatal_crash_events_observer_reported_local_id_manager.h"

#include <algorithm>
#include <iterator>
#include <sstream>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/sequence_checker.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/task/task_runner.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/fatal_crash/fatal_crash_events_observer.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"

namespace reporting {

FatalCrashEventsObserver::ReportedLocalIdManager::ReportedLocalIdManager(
    base::FilePath save_file_path,
    SaveFileLoadedCallback save_file_loaded_callback,
    scoped_refptr<base::SequencedTaskRunner> io_task_runner)
    : save_file_{std::move(save_file_path)},
      save_file_loaded_callback_{std::move(save_file_loaded_callback)},
      io_task_runner_{io_task_runner == nullptr
                          ? base::ThreadPool::CreateSequencedTaskRunner(
                                {base::TaskShutdownBehavior::BLOCK_SHUTDOWN,
                                 base::MayBlock()})
                          : std::move(io_task_runner)} {
  LoadSaveFile();
}

FatalCrashEventsObserver::ReportedLocalIdManager::~ReportedLocalIdManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

// static
std::unique_ptr<FatalCrashEventsObserver::ReportedLocalIdManager>
FatalCrashEventsObserver::ReportedLocalIdManager::Create(
    base::FilePath save_file_path,
    SaveFileLoadedCallback save_file_loaded_callback,
    scoped_refptr<base::SequencedTaskRunner> io_task_runner) {
  return base::WrapUnique(new ReportedLocalIdManager(
      std::move(save_file_path), std::move(save_file_loaded_callback),
      io_task_runner));
}

bool FatalCrashEventsObserver::ReportedLocalIdManager::HasBeenReported(
    const std::string& local_id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::Contains(local_ids_, local_id);
}

FatalCrashEventsObserver::ReportedLocalIdManager::ShouldReportResult
FatalCrashEventsObserver::ReportedLocalIdManager::ShouldReport(
    const std::string& local_id,
    int64_t capture_timestamp_us) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (capture_timestamp_us < 0) {
    // Only possible when loading a corrupt save file.
    LOG(ERROR) << "Negative timestamp found: " << local_id << ','
               << capture_timestamp_us;
    return ShouldReportResult::kNegativeTimestamp;
  }

  // Local ID already reported.
  if (HasBeenReported(local_id)) {
    return ShouldReportResult::kHasBeenReported;
  }

  // Max number of crash events reached and the current crash event is too old.
  if (local_ids_.size() >= kMaxNumOfLocalIds &&
      capture_timestamp_us <= GetEarliestLocalIdEntry().capture_timestamp_us) {
    return ShouldReportResult::kCrashTooOldAndMaxNumOfSavedLocalIdsReached;
  }

  return ShouldReportResult::kYes;
}

bool FatalCrashEventsObserver::ReportedLocalIdManager::UpdateLocalId(
    const std::string& local_id,
    int64_t capture_timestamp_us) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const auto result = UpdateLocalIdInRam(local_id, capture_timestamp_us);
  WriteSaveFile();
  return result;
}

bool FatalCrashEventsObserver::ReportedLocalIdManager::UpdateLocalIdInRam(
    const std::string& local_id,
    int64_t capture_timestamp_us) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (ShouldReport(local_id, capture_timestamp_us) !=
      ReportedLocalIdManager::ShouldReportResult::kYes) {
    return false;
  }

  // Keep only the most recent kMaxNumOfLocalIds local IDs. Remove that oldest
  // local IDs if too many are saved.
  if (local_ids_.size() >= kMaxNumOfLocalIds) {
    RemoveEarliestLocalIdEntry();
  }

  return Add(local_id, capture_timestamp_us);
}

bool FatalCrashEventsObserver::ReportedLocalIdManager::Add(
    const std::string& local_id,
    int64_t capture_timestamp_us) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Clean up before adding more entries. Otherwise it is possible that the
  // queue can grow uncontrolled.
  CleanUpLocalIdEntryQueue();
  if (!local_ids_.try_emplace(local_id, capture_timestamp_us).second) {
    return false;
  }
  local_id_entry_queue_.emplace(local_id, capture_timestamp_us);

  return true;
}

void FatalCrashEventsObserver::ReportedLocalIdManager::Remove(
    const std::string& local_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  local_ids_.erase(local_id);
  WriteSaveFile();
}

void FatalCrashEventsObserver::ReportedLocalIdManager::LoadSaveFile() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  io_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          [](base::FilePath save_file) -> std::string {
            if (!base::PathExists(save_file)) {
              // File has never been written yet, skip loading it.
              return std::string();
            }

            std::string content;
            if (!base::ReadFileToString(save_file, &content)) {
              LOG(ERROR) << "Failed to read save file: " << save_file;
              return std::string();
            }

            return content;
          },
          save_file_),
      base::BindOnce(&ReportedLocalIdManager::ResumeLoadingSaveFile,
                     weak_factory_.GetWeakPtr()));
}

void FatalCrashEventsObserver::ReportedLocalIdManager::ResumeLoadingSaveFile(
    const std::string& content) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(save_file_loaded_callback_);

  absl::Cleanup run_callback_on_return = [this] {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    save_file_loaded_ = true;
    std::move(save_file_loaded_callback_).Run();
  };

  // Parse the CSV file line by line. If one line is erroneous, stop parsing the
  // rest.
  for (const auto line : base::SplitStringPiece(
           content, "\n", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    // Parse.
    const auto csv_line_items = base::SplitStringPiece(
        base::TrimWhitespaceASCII(line, base::TRIM_ALL), ",",
        base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
    if (csv_line_items.size() != 2u) {
      LOG(ERROR) << "CSV line does not contain 2 rows: " << line;
      return;
    }

    const auto local_id = csv_line_items[0];
    int64_t capture_timestamp_us;
    if (!base::StringToInt64(csv_line_items[1], &capture_timestamp_us)) {
      LOG(ERROR) << "Failed to parse the timestamp: " << line;
      return;
    }

    // Load to RAM.
    if (!UpdateLocalIdInRam(std::string(local_id), capture_timestamp_us)) {
      LOG(ERROR) << "Not able to add the current crash: " << line;
      return;
    }
  }
}

void FatalCrashEventsObserver::ReportedLocalIdManager::WriteSaveFile() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Create the content of the CSV.
  std::ostringstream csv_content;
  for (const auto& [local_id, capture_timestamp_us] : local_ids_) {
    csv_content << local_id << ',' << capture_timestamp_us << '\n';
  }

  io_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](base::FilePath save_file, base::FilePath save_file_tmp,
             std::string content, uint64_t cur_save_file_writing_task_id,
             const std::atomic<uint64_t>* latest_save_file_writing_task_id) {
            if (cur_save_file_writing_task_id <
                latest_save_file_writing_task_id->load()) {
              // Another file writing task has been posted. Skip this one.
              return;
            }
            // Write to the temp save file first, then rename it to the official
            // save file. This would prevent partly written file to be
            // effective, as renaming within the same partition is atomic on
            // POSIX systems.
            if (!base::WriteFile(save_file_tmp, content)) {
              LOG(ERROR) << "Failed to write save file " << save_file_tmp;
              return;
            }
            if (base::File::Error err;
                !base::ReplaceFile(save_file_tmp, save_file, &err)) {
              LOG(ERROR) << "Failed to move file from " << save_file_tmp
                         << " to " << save_file << ": " << err;
              return;
            }
            // Successfully written the save file.
          },
          save_file_, save_file_tmp_, std::move(csv_content).str(),
          latest_save_file_writing_task_id_->load() + 1u,
          latest_save_file_writing_task_id_.get()));

  // Increase the latest task ID only after the latest task has been posted, not
  // before. Otherwise, in a rare case that this thread hangs after the latest
  // task ID has increased, the IO thread would prematurely skip all file
  // writing tasks.
  latest_save_file_writing_task_id_->fetch_add(1u);
}

void FatalCrashEventsObserver::ReportedLocalIdManager::
    CleanUpLocalIdEntryQueue() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (local_id_entry_queue_.size() >= kMaxSizeOfLocalIdEntryQueue) {
    // In extreme situations, such as when the oldest unuploaded crash remains
    // unuploaded for an extended amount of time, it's possible to leave a lot
    // of uploaded crashes' local IDs in local_id_entry_queue_. In this case,
    // rebuild the queue.
    ReconstructLocalIdEntries();
    return;
  }

  // Clean up uploaded crashes from the top of the priority queue.
  while (!local_id_entry_queue_.empty()) {
    if (base::Contains(local_ids_, local_id_entry_queue_.top().local_id)) {
      break;
    }
    local_id_entry_queue_.pop();
  }
}

void FatalCrashEventsObserver::ReportedLocalIdManager::
    ReconstructLocalIdEntries() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // First build the container.
  std::vector<LocalIdEntry> queue_container;
  queue_container.reserve(local_ids_.size());
  std::transform(local_ids_.begin(), local_ids_.end(),
                 std::back_inserter(queue_container),
                 [](const std::pair<std::string, int64_t>& input) {
                   return LocalIdEntry{.local_id = input.first,
                                       .capture_timestamp_us = input.second};
                 });

  // Then reconstruct the local ID entries priority queue from the container.
  local_id_entry_queue_ = decltype(local_id_entry_queue_)(
      decltype(local_id_entry_queue_)::value_compare(),
      std::move(queue_container));
}

const FatalCrashEventsObserver::LocalIdEntry&
FatalCrashEventsObserver::ReportedLocalIdManager::GetEarliestLocalIdEntry() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CleanUpLocalIdEntryQueue();
  // After the cleanup, the top of `local_id_entry_queue_` is the earliest.
  return local_id_entry_queue_.top();
}

void FatalCrashEventsObserver::ReportedLocalIdManager::
    RemoveEarliestLocalIdEntry() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CleanUpLocalIdEntryQueue();
  // After the cleanup, the top of `local_id_entry_queue_` is the earliest.
  local_ids_.erase(local_id_entry_queue_.top().local_id);
  local_id_entry_queue_.pop();
}

bool FatalCrashEventsObserver::ReportedLocalIdManager::IsSaveFileLoaded()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return save_file_loaded_;
}

bool FatalCrashEventsObserver::ReportedLocalIdManager::LocalIdEntryComparator::
operator()(const LocalIdEntry& a, const LocalIdEntry& b) const {
  return a.capture_timestamp_us > b.capture_timestamp_us;
}

}  // namespace reporting
