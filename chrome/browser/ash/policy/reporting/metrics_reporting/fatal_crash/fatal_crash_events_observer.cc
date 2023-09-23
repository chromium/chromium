// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/fatal_crash/fatal_crash_events_observer.h"

#include <sstream>
#include <string>
#include <string_view>
#include <utility>

#include "ash/public/cpp/session/session_types.h"
#include "ash/shell.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "chromeos/ash/services/cros_healthd/public/cpp/service_connection.h"
#include "components/reporting/proto/synced/metric_data.pb.h"
#include "components/user_manager/user_type.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace reporting {

using ::ash::cros_healthd::mojom::CrashEventInfo;
using ::ash::cros_healthd::mojom::CrashEventInfoPtr;

namespace {

constexpr std::string_view kDefaultReportedLocalIdFilePath =
    "/var/lib/reporting/crash_events/REPORTED_LOCAL_IDS";

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
          base::FilePath(kDefaultReportedLocalIdFilePath)) {}

FatalCrashEventsObserver::FatalCrashEventsObserver(
    base::FilePath reported_local_id_save_file)
    : MojoServiceEventsObserverBase<ash::cros_healthd::mojom::EventObserver>(
          this),
      reported_local_id_manager_{ReportedLocalIdManager::Create(
          std::move(reported_local_id_save_file))} {}

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

void FatalCrashEventsObserver::SetSkippedCrashCallback(
    base::RepeatingCallback<void(LocalIdEntry)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  skipped_callback_ = std::move(callback);
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
      skipped_callback_.Run({.local_id = std::move(crash_event_info->local_id),
                             .capture_timestamp_us = capture_timestamp_us});
      return;
    }
  }
  // TODO(b/266018440): If the crash is found to have been uploaded, need to
  // remove it from reported local IDs.

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
  }

  // TODO(b/266018440): was_reported_without_id is not filled. It involves logic
  // related to determining whether a crash event should be reported.

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

bool FatalCrashEventsObserver::ReportedLocalIdManager::ShouldReport(
    const std::string& local_id,
    int64_t capture_timestamp_us) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(local_id_entries_.size(), local_ids_.size());

  if (capture_timestamp_us < 0) {
    // Only possible when loading a corrupt save file.
    LOG(ERROR) << "Negative timestamp found: " << local_id << ','
               << capture_timestamp_us;
    return false;
  }

  // Local ID already reported.
  if (local_ids_.find(local_id) != local_ids_.end()) {
    return false;
  }
  // Max number of crash events reached and the current crash event is too old.
  if (local_id_entries_.size() >= kMaxNumOfLocalIds &&
      capture_timestamp_us <= local_id_entries_.top().capture_timestamp_us) {
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

  // Remove the oldest local ID if too many local IDs are saved.
  if (local_ids_.size() >= kMaxNumOfLocalIds) {
    local_ids_.erase(local_id_entries_.top().local_id);
    local_id_entries_.pop();
  }

  CHECK(local_ids_.try_emplace(local_id, capture_timestamp_us).second)
      << "Local ID " << local_id << " already saved while trying to emplace.";
  local_id_entries_.emplace(local_id, capture_timestamp_us);
  WriteSaveFile();

  return true;
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

bool FatalCrashEventsObserver::ReportedLocalIdManager::LocalIdEntryComparator::
operator()(const LocalIdEntry& a, const LocalIdEntry& b) const {
  return a.capture_timestamp_us > b.capture_timestamp_us;
}

}  // namespace reporting
