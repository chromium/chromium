// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/fatal_crash/fatal_crash_events_observer.h"

#include <string>
#include <string_view>
#include <utility>

#include "ash/public/cpp/session/session_types.h"
#include "ash/shell.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/fatal_crash/fatal_crash_events_observer_reported_local_id_manager.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/fatal_crash/fatal_crash_events_observer_uploaded_crash_info_manager.h"
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
constexpr base::TimeDelta kDefaultBackoffTimeForLoading = base::Seconds(5);

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
          base::FilePath(kDefaultUploadedCrashInfoSaveFilePath),
          kDefaultBackoffTimeForLoading) {}

FatalCrashEventsObserver::FatalCrashEventsObserver(
    base::FilePath reported_local_id_save_file,
    base::FilePath uploaded_crash_info_save_file,
    base::TimeDelta backoff_time_for_loading)
    : MojoServiceEventsObserverBase<ash::cros_healthd::mojom::EventObserver>(
          this),
      reported_local_id_manager_{ReportedLocalIdManager::Create(
          std::move(reported_local_id_save_file))},
      uploaded_crash_info_manager_{UploadedCrashInfoManager::Create(
          std::move(uploaded_crash_info_save_file))},
      backoff_time_for_loading_{backoff_time_for_loading} {}

FatalCrashEventsObserver::~FatalCrashEventsObserver() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

// static
std::unique_ptr<FatalCrashEventsObserver> FatalCrashEventsObserver::Create() {
  return base::WrapUnique(new FatalCrashEventsObserver());
}

// static
int64_t FatalCrashEventsObserver::ConvertTimeToMicroseconds(base::Time t) {
  return t.InMillisecondsSinceUnixEpoch() *
         base::Time::kMicrosecondsPerMillisecond;
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
    ash::cros_healthd::mojom::EventInfoPtr info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!AreLoaded()) {
    // If save files are still being loaded, wait for
    // `backoff_time_for_loading_` (5 seconds in production code).
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&FatalCrashEventsObserver::OnEvent,
                       weak_factory_.GetWeakPtr(), std::move(info)),
        backoff_time_for_loading_);
    return;
  }

  if (!info->is_crash_event_info()) {
    return;
  }
  const auto& crash_event_info = info->get_crash_event_info();

  if (crash_event_info->upload_info.is_null()) {
    // Unuploaded crash. Need to look up whether the crash has been reported or
    // not.
    const auto capture_timestamp_us =
        ConvertTimeToMicroseconds(crash_event_info->capture_time);
    const auto should_report_result = reported_local_id_manager_->ShouldReport(
        crash_event_info->local_id, capture_timestamp_us);
    // Currently impossible to reach `ShouldReportResult::kNegativeTimestamp`,
    // as it can only happen when loading a save file.
    base::UmaHistogramEnumeration(kUmaUnuploadedCrashShouldNotReportReason,
                                  should_report_result);
    if (should_report_result !=
        ReportedLocalIdManager::ShouldReportResult::kYes) {
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

bool FatalCrashEventsObserver::AreLoaded() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(b/266018440): Also off-load uploaded_crash_info_manager_'s save file.
  return reported_local_id_manager_->IsLoaded();
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
}  // namespace reporting
