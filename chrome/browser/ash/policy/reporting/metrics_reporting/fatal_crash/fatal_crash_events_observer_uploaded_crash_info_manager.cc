// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/fatal_crash/fatal_crash_events_observer_uploaded_crash_info_manager.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/fatal_crash/fatal_crash_events_observer.h"

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
#include "base/time/time.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_events.mojom.h"

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
    base::FilePath save_file_path) {
  return base::WrapUnique(
      new UploadedCrashInfoManager(std::move(save_file_path)));
}

FatalCrashEventsObserver::UploadedCrashInfoManager::UploadedCrashInfoManager(
    base::FilePath save_file_path)
    : save_file_{std::move(save_file_path)} {
  auto result = LoadSaveFile();
  if (!result.has_value()) {
    LOG(ERROR) << result.error();
    return;
  }

  uploads_log_creation_time_ = base::Time::FromMillisecondsSinceUnixEpoch(
      result.value().uploads_log_creation_timestamp_ms);
  uploads_log_offset_ = result.value().uploads_log_offset;
}

FatalCrashEventsObserver::UploadedCrashInfoManager::
    ~UploadedCrashInfoManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

base::expected<FatalCrashEventsObserver::UploadedCrashInfoManager::ParseResult,
               Status>
FatalCrashEventsObserver::UploadedCrashInfoManager::LoadSaveFile() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::string content;
  if (!base::ReadFileToString(save_file_, &content)) {
    return base::unexpected(Status(
        error::INTERNAL,
        base::StrCat({"Failed to read save file: ", save_file_.value()})));
  }

  const auto parsed_result =
      base::JSONReader::ReadAndReturnValueWithError(content);
  if (!parsed_result.has_value()) {
    return base::unexpected(Status(
        error::INTERNAL,
        base::StrCat({"Failed to parse the save file ", save_file_.value(),
                      " as JSON: ", parsed_result.error().ToString()})));
  }

  const auto* const dict_result = parsed_result.value().GetIfDict();
  if (dict_result == nullptr) {
    return base::unexpected(
        Status(error::INTERNAL,
               base::StrCat({"Parsed JSON string is not a dict: ",
                             TruncateString(content, /*size=*/200u)})));
  }

  const auto* const creation_timestamp_ms_string =
      dict_result->FindString(kCreationTimestampMsJsonKey);
  if (creation_timestamp_ms_string == nullptr) {
    return base::unexpected(Status(
        error::INTERNAL,
        base::StrCat(
            {"Creation timestamp key ", kCreationTimestampMsJsonKey,
             " not found in JSON: ", TruncateString(content, /*size=*/200u)})));
  }

  ParseResult result;

  if (!base::StringToInt64(*creation_timestamp_ms_string,
                           &result.uploads_log_creation_timestamp_ms)) {
    return base::unexpected(
        Status(error::INTERNAL,
               base::StrCat({"Failed to convert timestamp ",
                             *creation_timestamp_ms_string, " to int64."})));
  }

  if (result.uploads_log_creation_timestamp_ms < 0) {
    return base::unexpected(
        Status(error::INTERNAL,
               base::StrCat({"Timestamp ", *creation_timestamp_ms_string,
                             " is negative."})));
  }

  const auto* const offset_string = dict_result->FindString(kOffsetJsonKey);
  if (offset_string == nullptr) {
    return base::unexpected(Status(
        error::INTERNAL,
        base::StrCat({"Offset key ", kOffsetJsonKey, " not found in JSON: ",
                      TruncateString(content, /*size=*/200u)})));
  }

  if (!base::StringToUint64(*offset_string, &result.uploads_log_offset)) {
    return base::unexpected(
        Status(error::INTERNAL, base::StrCat({"Failed to convert offset ",
                                              *offset_string, " to uint64."})));
  }

  // Ignore additional keys here even if they are present, so as to keep
  // slightly better flexibility when more fields are added to this JSON file
  // (thus reversion won't break the current code).

  return result;
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

  // Write to the temp save file first, then rename it to the official save
  // file. This would prevent partly written file to be effective, as renaming
  // within the same partition is atomic on POSIX systems.
  if (!base::WriteFile(save_file_tmp_, content.value())) {
    return Status(error::INTERNAL, base::StrCat({"Failed to write save file ",
                                                 save_file_tmp_.value()}));
  }
  if (base::File::Error err;
      !base::ReplaceFile(save_file_tmp_, save_file_, &err)) {
    std::ostringstream err_msg;
    err_msg << "Failed to move file from " << save_file_tmp_ << " to "
            << save_file_ << ": " << err;
    return Status(error::INTERNAL, err_msg.str());
  }

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
}  // namespace reporting
