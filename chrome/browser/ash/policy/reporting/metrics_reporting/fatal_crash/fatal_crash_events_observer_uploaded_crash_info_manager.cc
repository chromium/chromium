// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/fatal_crash/fatal_crash_events_observer_uploaded_crash_info_manager.h"

#include <atomic>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/sequence_checker.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/fatal_crash/fatal_crash_events_observer.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_events.mojom.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/statusor.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"

namespace reporting {

using ::ash::cros_healthd::mojom::CrashUploadInfoPtr;

namespace {
// Truncates a string to a maximum of length of `size`.
[[nodiscard]] std::string_view TruncateString(std::string_view str,
                                              size_t size) {
  return str.substr(0u, size);
}
}  // namespace

// static
std::unique_ptr<FatalCrashEventsObserver::UploadedCrashInfoManager>
FatalCrashEventsObserver::UploadedCrashInfoManager::Create(
    base::FilePath save_file_path,
    SaveFileLoadedCallback save_file_loaded_callback,
    scoped_refptr<base::SequencedTaskRunner> io_task_runner) {
  return base::WrapUnique(new UploadedCrashInfoManager(
      std::move(save_file_path), std::move(save_file_loaded_callback),
      std::move(io_task_runner)));
}

FatalCrashEventsObserver::UploadedCrashInfoManager::UploadedCrashInfoManager(
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

FatalCrashEventsObserver::UploadedCrashInfoManager::
    ~UploadedCrashInfoManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void FatalCrashEventsObserver::UploadedCrashInfoManager::LoadSaveFile() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  io_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          [](base::FilePath save_file) -> StatusOr<std::string> {
            if (!base::PathExists(save_file)) {
              // File has never been written yet, skip loading it.
              return std::string();
            }

            std::string content;
            if (!base::ReadFileToString(save_file, &content)) {
              std::string error_message = base::StrCat(
                  {"Failed to read save file: ", save_file.value()});
              LOG(ERROR) << error_message;
              return base::unexpected(
                  Status(error::INTERNAL, std::move(error_message)));
            }

            return content;
          },
          save_file_),
      base::BindOnce(&UploadedCrashInfoManager::ResumeLoadingSaveFile,
                     weak_factory_.GetWeakPtr()));
}

void FatalCrashEventsObserver::UploadedCrashInfoManager::ResumeLoadingSaveFile(
    const StatusOr<std::string>& content) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(save_file_loaded_callback_);

  absl::Cleanup run_callback_on_return = [this] {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    save_file_loaded_ = true;
    std::move(save_file_loaded_callback_).Run();
  };

  if (!content.has_value()) {
    // Error already logged immediately after reading file fails.
    return;
  }

  const auto parsed_result =
      base::JSONReader::ReadAndReturnValueWithError(content.value());
  if (!parsed_result.has_value()) {
    LOG(ERROR) << "Failed to parse the save file " << save_file_.value()
               << " as JSON: " << parsed_result.error().ToString();
    return;
  }

  const auto* const dict_result = parsed_result.value().GetIfDict();
  if (dict_result == nullptr) {
    LOG(ERROR) << "Parsed JSON string is not a dict: "
               << TruncateString(content.value(), /*size=*/200u);
    return;
  }

  const auto* const creation_timestamp_ms_string =
      dict_result->FindString(kCreationTimestampMsJsonKey);
  if (creation_timestamp_ms_string == nullptr) {
    LOG(ERROR) << "Creation timestamp key " << kCreationTimestampMsJsonKey
               << " not found in JSON: "
               << TruncateString(content.value(), /*size=*/200u);
    return;
  }

  ParseResult result;

  if (!base::StringToInt64(*creation_timestamp_ms_string,
                           &result.uploads_log_creation_timestamp_ms)) {
    LOG(ERROR) << "Failed to convert timestamp "
               << *creation_timestamp_ms_string << " to int64.";
    return;
  }

  if (result.uploads_log_creation_timestamp_ms < 0) {
    LOG(ERROR) << "Timestamp " << *creation_timestamp_ms_string
               << " is negative.";
    return;
  }

  const auto* const offset_string = dict_result->FindString(kOffsetJsonKey);
  if (offset_string == nullptr) {
    LOG(ERROR) << "Offset key " << kOffsetJsonKey << " not found in JSON: "
               << TruncateString(content.value(), /*size=*/200u);
    return;
  }

  if (!base::StringToUint64(*offset_string, &result.uploads_log_offset)) {
    LOG(ERROR) << "Failed to convert offset " << *offset_string
               << " to uint64.";
    return;
  }

  // Ignore additional keys here even if they are present, so as to keep
  // slightly better flexibility when more fields are added to this JSON file
  // (thus reversion won't break the current code).

  uploads_log_creation_time_ = base::Time::FromMillisecondsSinceUnixEpoch(
      result.uploads_log_creation_timestamp_ms);
  uploads_log_offset_ = result.uploads_log_offset;
}

Status FatalCrashEventsObserver::UploadedCrashInfoManager::WriteSaveFile()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::Value::Dict info;
  info.Set(kCreationTimestampMsJsonKey,
           base::NumberToString(
               uploads_log_creation_time_.InMillisecondsSinceUnixEpoch()));
  info.Set(kOffsetJsonKey, base::NumberToString(uploads_log_offset_));

  auto content = base::WriteJson(info);
  if (!content.has_value()) {
    return Status(error::INTERNAL,
                  "Failed to create a JSON string for uploaded crash info");
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
          save_file_, save_file_tmp_, std::move(content).value(),
          latest_save_file_writing_task_id_->load() + 1u,
          latest_save_file_writing_task_id_.get()));

  // Increase the latest task ID only after the latest task has been posted, not
  // before. Otherwise, in a rare case that this thread hangs after the latest
  // task ID has increased, the IO thread would prematurely skip all file
  // writing tasks.
  latest_save_file_writing_task_id_->fetch_add(1u);

  return Status::StatusOK();
}

bool FatalCrashEventsObserver::UploadedCrashInfoManager::IsNewer(
    base::Time uploads_log_creation_time,
    uint64_t uploads_log_offset) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return std::tie(uploads_log_creation_time, uploads_log_offset) >
         std::tie(uploads_log_creation_time_, uploads_log_offset_);
}

bool FatalCrashEventsObserver::UploadedCrashInfoManager::ShouldReport(
    const CrashUploadInfoPtr& upload_info) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return IsNewer(upload_info->creation_time, upload_info->offset);
}

void FatalCrashEventsObserver::UploadedCrashInfoManager::Update(
    base::Time uploads_log_creation_time,
    uint64_t uploads_log_offset) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!IsNewer(uploads_log_creation_time, uploads_log_offset)) {
    return;
  }

  uploads_log_creation_time_ = uploads_log_creation_time;
  uploads_log_offset_ = uploads_log_offset;
  const Status status = WriteSaveFile();
  if (!status.ok()) {
    LOG(ERROR) << "Failed to write save file: " << status;
    return;
  }
}

bool FatalCrashEventsObserver::UploadedCrashInfoManager::IsSaveFileLoaded()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return save_file_loaded_;
}
}  // namespace reporting
