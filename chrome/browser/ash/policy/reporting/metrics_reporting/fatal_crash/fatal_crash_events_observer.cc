// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/fatal_crash/fatal_crash_events_observer.h"

#include <algorithm>
#include <iterator>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

#include "ash/public/cpp/session/session_types.h"
#include "ash/shell.h"
#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "chromeos/ash/services/cros_healthd/public/cpp/service_connection.h"
#include "components/reporting/proto/synced/metric_data.pb.h"
#include "components/user_manager/user_type.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace reporting {

using ::ash::cros_healthd::mojom::CrashEventInfo;
using ::ash::cros_healthd::mojom::CrashEventInfoPtr;
using ::ash::cros_healthd::mojom::CrashUploadInfoPtr;

namespace {

constexpr std::string_view kDefaultReportedLocalIdSaveFilePath =
    "/var/lib/reporting/crash_events/REPORTED_LOCAL_IDS";
constexpr std::string_view kDefaultUploadedCrashInfoSaveFilePath =
    "/var/lib/reporting/crash_events/UPLOADED_CRASH_INFO";

// Truncates a string to a maximum of length of `size`.
[[nodiscard]] std::string_view TruncateString(std::string_view str,
                                              size_t size) {
  return str.substr(0u, size);
}

// Get current user session.
const ash::UserSession* GetCurrentUserSession() {
  return ash::Shell::Get()->session_controller()->GetPrimaryUserSession();
}

// Get the type of the given session.
FatalCrashTelemetry::SessionType GetSessionType(
    const ash::UserSession* user_session) {
  if (!user_session) {
    return FatalCrashTelemetry::SESSION_TYPE_UNSPECIFIED;
  }
  switch (user_session->user_info.type) {
    case user_manager::USER_TYPE_REGULAR:
      return FatalCrashTelemetry::SESSION_TYPE_REGULAR;
    case user_manager::USER_TYPE_CHILD:
      return FatalCrashTelemetry::SESSION_TYPE_CHILD;
    case user_manager::USER_TYPE_GUEST:
      return FatalCrashTelemetry::SESSION_TYPE_GUEST;
    case user_manager::USER_TYPE_PUBLIC_ACCOUNT:
      return FatalCrashTelemetry::SESSION_TYPE_PUBLIC_ACCOUNT;
    case user_manager::USER_TYPE_KIOSK_APP:
      return FatalCrashTelemetry::SESSION_TYPE_KIOSK_APP;
    case user_manager::USER_TYPE_ARC_KIOSK_APP:
      return FatalCrashTelemetry::SESSION_TYPE_ARC_KIOSK_APP;
    case user_manager::USER_TYPE_WEB_KIOSK_APP:
      return FatalCrashTelemetry::SESSION_TYPE_WEB_KIOSK_APP;
    default:
      NOTREACHED_NORETURN();
  }
}

// Get the user email of the given session.
absl::optional<std::string> GetUserEmail(const ash::UserSession* user_session) {
  if (!user_session || !user_session->user_info.is_managed) {
    return absl::nullopt;
  }
  if (!user_session->user_info.account_id.is_valid()) {
    LOG(ERROR) << "Invalid user account ID.";
    return absl::nullopt;
  }
  return user_session->user_info.account_id.GetUserEmail();
}
}  // namespace

FatalCrashEventsObserver::FatalCrashEventsObserver()
    : FatalCrashEventsObserver(
          base::FilePath(kDefaultReportedLocalIdSaveFilePath),
          base::FilePath(kDefaultUploadedCrashInfoSaveFilePath)) {}

FatalCrashEventsObserver::FatalCrashEventsObserver(
    base::FilePath reported_local_id_save_file,
    base::FilePath uploaded_crash_info_save_file)
    : MojoServiceEventsObserverBase<ash::cros_healthd::mojom::EventObserver>(
          this),
      reported_local_id_manager_{ReportedLocalIdManager::Create(
          std::move(reported_local_id_save_file))},
      uploaded_crash_info_manager_{UploadedCrashInfoManager::Create(
          std::move(uploaded_crash_info_save_file))} {}

FatalCrashEventsObserver::~FatalCrashEventsObserver() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

// static
std::unique_ptr<FatalCrashEventsObserver> FatalCrashEventsObserver::Create() {
  return base::WrapUnique(new FatalCrashEventsObserver());
}

// static
int64_t FatalCrashEventsObserver::ConvertTimeToMicroseconds(base::Time t) {
  return t.ToJavaTime() * base::Time::kMicrosecondsPerMillisecond;
}

void FatalCrashEventsObserver::SetSkippedUnuploadedCrashCallback(
    base::RepeatingCallback<void(LocalIdEntry)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  skipped_unuploaded_callback_ = std::move(callback);
}

void FatalCrashEventsObserver::SetSkippedUploadedCrashCallback(
    SkippedUploadedCrashCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  skipped_uploaded_callback_ = std::move(callback);
}

void FatalCrashEventsObserver::OnEvent(
    const ash::cros_healthd::mojom::EventInfoPtr info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!info->is_crash_event_info()) {
    return;
  }
  const auto& crash_event_info = info->get_crash_event_info();

  if (crash_event_info->upload_info.is_null()) {
    // Unuploaded crash. Need to look up whether the crash has been reported or
    // not.
    if (auto capture_timestamp_us =
            ConvertTimeToMicroseconds(crash_event_info->capture_time);
        !reported_local_id_manager_->ShouldReport(crash_event_info->local_id,
                                                  capture_timestamp_us)) {
      // Crash is already reported. Skip.
      skipped_unuploaded_callback_.Run(
          {.local_id = std::move(crash_event_info->local_id),
           .capture_timestamp_us = capture_timestamp_us});
      return;
    }
  } else {
    // Uploaded crash.
    if (!uploaded_crash_info_manager_->ShouldReport(
            crash_event_info->upload_info)) {
      // The crash is from an earlier part of uploads.log. Skip.
      const auto& upload_info = crash_event_info->upload_info;
      skipped_uploaded_callback_.Run(upload_info->crash_report_id,
                                     upload_info->creation_time,
                                     upload_info->offset);
      return;
    }
  }

  MetricData metric_data = FillFatalCrashTelemetry(crash_event_info);
  OnEventObserved(std::move(metric_data));

  if (interrupted_after_event_observed_for_test_) {
    return;
  }

  if (crash_event_info->upload_info.is_null()) {
    // Unuploaded crash. Need to update saved reported local IDs.
    if (auto capture_timestamp_us =
            ConvertTimeToMicroseconds(crash_event_info->capture_time);
        !reported_local_id_manager_->UpdateLocalId(crash_event_info->local_id,
                                                   capture_timestamp_us)) {
      LOG(ERROR) << "Failed to update local ID: " << crash_event_info->local_id;
      return;
    }
  } else {
    // Uploaded crash.
    uploaded_crash_info_manager_->Update(
        crash_event_info->upload_info->creation_time,
        crash_event_info->upload_info->offset);
    // Once uploaded, the crash's local ID must be removed from saved local IDs.
    // Reason is that when the number of saved local IDs reach the max, the
    // crash with the earliest capture time will be removed. However, crashes do
    // not come in the order of capture time. If we leave uploaded crashes in
    // the saved local IDs, some late-coming crashes with early capture time may
    // not get reported because of this.
    reported_local_id_manager_->Remove(crash_event_info->local_id);
  }
}

void FatalCrashEventsObserver::AddObserver() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ash::cros_healthd::ServiceConnection::GetInstance()
      ->GetEventService()
      ->AddEventObserver(ash::cros_healthd::mojom::EventCategoryEnum::kCrash,
                         BindNewPipeAndPassRemote());
}

MetricData FatalCrashEventsObserver::FillFatalCrashTelemetry(
    const CrashEventInfoPtr& info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  MetricData metric_data;
  FatalCrashTelemetry& data =
      *metric_data.mutable_telemetry_data()->mutable_fatal_crash_telemetry();

  switch (info->crash_type) {
    case CrashEventInfo::CrashType::kKernel:
      data.set_type(FatalCrashTelemetry::CRASH_TYPE_KERNEL);
      break;
    case CrashEventInfo::CrashType::kEmbeddedController:
      data.set_type(FatalCrashTelemetry::CRASH_TYPE_EMBEDDED_CONTROLLER);
      break;
    case CrashEventInfo::CrashType::kUnknown:
      [[fallthrough]];
    default:  // Other types added by healthD that are unknown here yet.
      data.set_type(FatalCrashTelemetry::CRASH_TYPE_UNSPECIFIED);
  }

  const auto* const user_session = GetCurrentUserSession();
  if (!user_session) {
    LOG(ERROR) << "Unable to obtain user session.";
  }
  data.set_session_type(GetSessionType(user_session));
  if (auto user_email = GetUserEmail(user_session); user_email.has_value()) {
    data.mutable_affiliated_user()->set_user_email(user_email.value());
  }

  *data.mutable_local_id() = info->local_id;
  data.set_timestamp_us(ConvertTimeToMicroseconds(info->capture_time));
  if (!info->upload_info.is_null()) {
    *data.mutable_crash_report_id() = info->upload_info->crash_report_id;
    // If reported_local_id_manager_->HasBeenReported indicates a crash with the
    // same local ID has been reported, it implies that it has been reported
    // once as an unuploaded crash, which does not have a crash report ID.
    data.set_been_reported_without_crash_report_id(
        reported_local_id_manager_->HasBeenReported(data.local_id()));
  }

  return metric_data;
}

void FatalCrashEventsObserver::SetInterruptedAfterEventObservedForTest(
    bool interrupted_after_event_observed) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  interrupted_after_event_observed_for_test_ = interrupted_after_event_observed;
}

FatalCrashEventsObserver::ReportedLocalIdManager::ReportedLocalIdManager(
    base::FilePath save_file_path)
    : save_file_{std::move(save_file_path)} {
  LoadSaveFile();
}

FatalCrashEventsObserver::ReportedLocalIdManager::~ReportedLocalIdManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

// static
std::unique_ptr<FatalCrashEventsObserver::ReportedLocalIdManager>
FatalCrashEventsObserver::ReportedLocalIdManager::Create(
    base::FilePath save_file_path) {
  return base::WrapUnique(
      new ReportedLocalIdManager(std::move(save_file_path)));
}

bool FatalCrashEventsObserver::ReportedLocalIdManager::HasBeenReported(
    const std::string& local_id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::Contains(local_ids_, local_id);
}

bool FatalCrashEventsObserver::ReportedLocalIdManager::ShouldReport(
    const std::string& local_id,
    int64_t capture_timestamp_us) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (capture_timestamp_us < 0) {
    // Only possible when loading a corrupt save file.
    LOG(ERROR) << "Negative timestamp found: " << local_id << ','
               << capture_timestamp_us;
    return false;
  }

  // Local ID already reported.
  if (HasBeenReported(local_id)) {
    return false;
  }

  // Max number of crash events reached and the current crash event is too old.
  if (local_ids_.size() >= kMaxNumOfLocalIds &&
      capture_timestamp_us <= GetEarliestLocalIdEntry().capture_timestamp_us) {
    return false;
  }

  return true;
}

bool FatalCrashEventsObserver::ReportedLocalIdManager::UpdateLocalId(
    const std::string& local_id,
    int64_t capture_timestamp_us) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!ShouldReport(local_id, capture_timestamp_us)) {
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

  WriteSaveFile();
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

  if (!base::PathExists(save_file_)) {
    // File has never been written yet, skip loading it.
    return;
  }

  std::string content;
  if (!base::ReadFileToString(save_file_, &content)) {
    LOG(ERROR) << "Failed to read save file: " << save_file_;
    return;
  }

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
    if (!UpdateLocalId(std::string(local_id), capture_timestamp_us)) {
      LOG(ERROR) << "Not able to add the current crash: " << line;
      return;
    }
  }
}

void FatalCrashEventsObserver::ReportedLocalIdManager::WriteSaveFile() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Create the content of the CSV.
  std::ostringstream csv_content;
  for (const auto& [local_id, capture_timestamp_us] : local_ids_) {
    csv_content << local_id << ',' << capture_timestamp_us << '\n';
  }

  // Write to the temp save file first, then rename it to the official save
  // file. This would prevent partly written file to be effective, as renaming
  // within the same partition is atomic on POSIX systems.
  if (!base::WriteFile(save_file_tmp_, csv_content.str())) {
    LOG(ERROR) << "Failed to write save file " << save_file_tmp_;
    return;
  }
  if (base::File::Error err;
      !base::ReplaceFile(save_file_tmp_, save_file_, &err)) {
    LOG(ERROR) << "Failed to move file from " << save_file_tmp_ << " to "
               << save_file_ << ": " << err;
    return;
  }
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

bool FatalCrashEventsObserver::ReportedLocalIdManager::LocalIdEntryComparator::
operator()(const LocalIdEntry& a, const LocalIdEntry& b) const {
  return a.capture_timestamp_us > b.capture_timestamp_us;
}

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

  uploads_log_creation_time_ = base::Time::FromJavaTime(
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
           base::NumberToString(uploads_log_creation_time_.ToJavaTime()));
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
