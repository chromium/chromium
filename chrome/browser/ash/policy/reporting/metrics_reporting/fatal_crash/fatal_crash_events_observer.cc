// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/fatal_crash/fatal_crash_events_observer.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "ash/public/cpp/session/session_types.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "chrome/browser/ash/policy/reporting/event_based_logs/event_based_log_uploader.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/fatal_crash/fatal_crash_events_observer_reported_local_id_manager.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/fatal_crash/fatal_crash_events_observer_settings_for_test.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/fatal_crash/fatal_crash_events_observer_uploaded_crash_info_manager.h"
#include "chromeos/ash/services/cros_healthd/public/cpp/service_connection.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_events.mojom.h"
#include "components/reporting/proto/synced/metric_data.pb.h"
#include "components/user_manager/user_type.h"

namespace reporting {

using ::ash::cros_healthd::mojom::CrashEventInfo;
using ::ash::cros_healthd::mojom::CrashEventInfoPtr;
using ::ash::cros_healthd::mojom::EventInfoPtr;

constexpr char kReportedLocalIdSaveFilePath[] =
    "/var/lib/reporting/crash_events/CRASH_REPORTED_LOCAL_IDS";
constexpr char kUploadedCrashInfoSaveFilePath[] =
    "/var/lib/reporting/crash_events/CRASH_UPLOADED_CRASH_INFO";

namespace {

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
    case user_manager::UserType::kRegular:
      return FatalCrashTelemetry::SESSION_TYPE_REGULAR;
    case user_manager::UserType::kChild:
      return FatalCrashTelemetry::SESSION_TYPE_CHILD;
    case user_manager::UserType::kGuest:
      return FatalCrashTelemetry::SESSION_TYPE_GUEST;
    case user_manager::UserType::kPublicAccount:
      return FatalCrashTelemetry::SESSION_TYPE_PUBLIC_ACCOUNT;
    case user_manager::UserType::kKioskApp:
      return FatalCrashTelemetry::SESSION_TYPE_KIOSK_APP;
    case user_manager::UserType::kWebKioskApp:
    // TODO(crbug.com/358536558): Process a new user type for IWA kiosk
    case user_manager::UserType::kKioskIWA:
      return FatalCrashTelemetry::SESSION_TYPE_WEB_KIOSK_APP;
    default:
      NOTREACHED();
  }
}

// Get the user email of the given session.
std::optional<std::string> GetUserEmail(const ash::UserSession* user_session) {
  if (!user_session || !user_session->user_info.is_managed) {
    return std::nullopt;
  }
  if (!user_session->user_info.account_id.is_valid()) {
    LOG(ERROR) << "Invalid user account ID.";
    return std::nullopt;
  }
  return user_session->user_info.account_id.GetUserEmail();
}
}  // namespace

FatalCrashEventsObserver::FatalCrashEventsObserver()
    : FatalCrashEventsObserver(base::FilePath(kReportedLocalIdSaveFilePath),
                               base::FilePath(kUploadedCrashInfoSaveFilePath),
                               /*reported_local_id_io_task_runner=*/nullptr,
                               /*uploaded_crash_info_io_task_runner=*/nullptr) {
}

FatalCrashEventsObserver::FatalCrashEventsObserver(
    const base::FilePath& reported_local_id_save_file_path,
    const base::FilePath& uploaded_crash_info_save_file_path,
    scoped_refptr<base::SequencedTaskRunner> reported_local_id_io_task_runner,
    scoped_refptr<base::SequencedTaskRunner> uploaded_crash_info_io_task_runner)
    : MojoServiceEventsObserverBase<ash::cros_healthd::mojom::EventObserver>(
          this),
      reported_local_id_manager_{ReportedLocalIdManager::Create(
          reported_local_id_save_file_path,
          // Don't BindPostTask here, because it would risk calling
          // `ProcessEventsBeforeSaveFilesLoaded` twice, once from
          // reported_local_id_manager_, once from uploaded_crash_info_manager_.
          /*save_file_loaded_callback=*/
          base::BindOnce(
              &FatalCrashEventsObserver::ProcessEventsBeforeSaveFilesLoaded,
              // Called from member reported_local_id_manager_ from
              // the same sequence, safe to assume this instance is still alive.
              base::Unretained(this)),
          std::move(reported_local_id_io_task_runner))},
      uploaded_crash_info_manager_{UploadedCrashInfoManager::Create(
          uploaded_crash_info_save_file_path,
          // Don't BindPostTask here, because it would risk calling
          // `ProcessEventsBeforeSaveFilesLoaded` twice, once from
          // reported_local_id_manager_, once from uploaded_crash_info_manager_.
          /*save_file_loaded_callback=*/
          base::BindOnce(
              &FatalCrashEventsObserver::ProcessEventsBeforeSaveFilesLoaded,
              // Called from member uploaded_crash_info_manager_ from
              // the same sequence, safe to assume this instance is still alive.
              base::Unretained(this)),
          std::move(uploaded_crash_info_io_task_runner))} {}

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

const base::flat_set<CrashEventInfo::CrashType>&
FatalCrashEventsObserver::GetAllowedCrashTypes() const {
  // This may appear to be overkilling for only 2 crash types, but it provides
  // more robustness for future crash type additions.
  static const base::NoDestructor<base::flat_set<CrashEventInfo::CrashType>>
      allowed_crash_types({CrashEventInfo::CrashType::kKernel,
                           CrashEventInfo::CrashType::kEmbeddedController});
  return *allowed_crash_types;
}

void FatalCrashEventsObserver::OnEvent(EventInfoPtr info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_CALLED_ON_VALID_SEQUENCE(settings_for_test_->sequence_checker);

  if (!info->is_crash_event_info()) {
    return;
  }

  // Events in `event_queue_before_save_files_loaded_` must be processed first.
  // If the events there have not been cleared, enqueue this event there.
  if (!AreSaveFilesLoaded() || !event_queue_before_save_files_loaded_.empty()) {
    if (settings_for_test_->event_collected_before_save_files_loaded_callback) {
      settings_for_test_->event_collected_before_save_files_loaded_callback.Run(
          info->get_crash_event_info().Clone());
    }
    event_queue_before_save_files_loaded_.push(std::move(info));
    return;
  }

  ProcessEvent(std::move(info));
}

void FatalCrashEventsObserver::ProcessEvent(EventInfoPtr info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_CALLED_ON_VALID_SEQUENCE(settings_for_test_->sequence_checker);

  auto& crash_event_info = info->get_crash_event_info();
  if (!GetAllowedCrashTypes().contains(crash_event_info->crash_type)) {
    // A type of crash that is uninteresting to us. Don't process it.
    settings_for_test_->skipped_uninteresting_crash_type_callback.Run(
        crash_event_info->crash_type);
    return;
  }

  if (crash_event_info->upload_info.is_null()) {
    ProcessUnuploadedCrashEvent(std::move(crash_event_info));
  } else {
    ProcessUploadedCrashEvent(std::move(crash_event_info));
  }
}

void FatalCrashEventsObserver::ProcessUnuploadedCrashEvent(
    CrashEventInfoPtr crash_event_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_CALLED_ON_VALID_SEQUENCE(settings_for_test_->sequence_checker);

  // Look up whether the crash has been reported or not.
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
    settings_for_test_->skipped_unuploaded_crash_callback.Run(
        {.local_id = std::move(crash_event_info->local_id),
         .capture_timestamp_us = capture_timestamp_us});
    return;
  }

  // `event_based_log_upload_id` will only be generated for events with uploaded
  // crash reports.
  MetricData metric_data = FillFatalCrashTelemetry(
      crash_event_info, /*event_based_log_upload_id=*/std::nullopt);
  OnEventObserved(std::move(metric_data));
  if (settings_for_test_->interrupted_after_event_observed) {
    return;
  }

  // Update saved reported local IDs.
  if (!reported_local_id_manager_->UpdateLocalId(crash_event_info->local_id,
                                                 capture_timestamp_us)) {
    LOG(ERROR) << "Failed to update local ID: " << crash_event_info->local_id;
    return;
  }
}

void FatalCrashEventsObserver::ProcessUploadedCrashEvent(
    CrashEventInfoPtr crash_event_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_CALLED_ON_VALID_SEQUENCE(settings_for_test_->sequence_checker);

  if (!uploaded_crash_info_manager_->ShouldReport(
          crash_event_info->upload_info)) {
    // The crash is from an earlier part of uploads.log. Skip.
    const auto& upload_info = crash_event_info->upload_info;
    settings_for_test_->skipped_uploaded_crash_callback.Run(
        upload_info->crash_report_id, upload_info->creation_time,
        upload_info->offset);
    return;
  }

  auto event_based_log_upload_id = NotifyFatalCrashEventLog();
  MetricData metric_data =
      FillFatalCrashTelemetry(crash_event_info, event_based_log_upload_id);
  OnEventObserved(std::move(metric_data));

  if (settings_for_test_->interrupted_after_event_observed) {
    return;
  }

  uploaded_crash_info_manager_->Update(
      crash_event_info->upload_info->creation_time,
      crash_event_info->upload_info->offset);
  // Once uploaded, the crash's local ID must be removed from saved local IDs.
  // Reason is that when the number of saved local IDs reach the max, the crash
  // with the earliest capture time will be removed. However, crashes do not
  // come in the order of capture time. If we leave uploaded crashes in the
  // saved local IDs, some late-coming crashes with early capture time may not
  // get reported because of this.
  reported_local_id_manager_->Remove(crash_event_info->local_id);
}

void FatalCrashEventsObserver::AddObserver() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ash::cros_healthd::ServiceConnection::GetInstance()
      ->GetEventService()
      ->AddEventObserver(ash::cros_healthd::mojom::EventCategoryEnum::kCrash,
                         BindNewPipeAndPassRemote());
}

bool FatalCrashEventsObserver::AreSaveFilesLoaded() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return reported_local_id_manager_->IsSaveFileLoaded() &&
         uploaded_crash_info_manager_->IsSaveFileLoaded();
}

void FatalCrashEventsObserver::ProcessEventsBeforeSaveFilesLoaded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!AreSaveFilesLoaded()) {
    // Don't do anything if not yet loaded. There are two save files,
    // REPORTED_LOCAL_IDS and UPLOADED_CRASH_INFO. This function is called when
    // either save file is loaded.
    //
    // The first call to this method would likely reach here as it is called
    // when the first save file is loaded, since the other file is not yet
    // loaded.
    return;
  }

  if (event_queue_before_save_files_loaded_.empty()) {
    return;
  }

  // Only crash events can be enqueued to `events_gathered_before_loaded_`.
  CHECK(event_queue_before_save_files_loaded_.front()->is_crash_event_info());

  ProcessEvent(std::move(event_queue_before_save_files_loaded_.front()));
  event_queue_before_save_files_loaded_.pop();

  // Process one crash event at a time to avoid blocking processing other types
  // of events.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &FatalCrashEventsObserver::ProcessEventsBeforeSaveFilesLoaded,
          weak_factory_.GetWeakPtr()));
}

FatalCrashTelemetry::CrashType
FatalCrashEventsObserver::GetFatalCrashTelemetryCrashType(
    CrashEventInfo::CrashType crash_type) const {
  switch (crash_type) {
    case CrashEventInfo::CrashType::kKernel:
      return FatalCrashTelemetry::CRASH_TYPE_KERNEL;
    case CrashEventInfo::CrashType::kEmbeddedController:
      return FatalCrashTelemetry::CRASH_TYPE_EMBEDDED_CONTROLLER;
    case CrashEventInfo::CrashType::kUnknown:
      [[fallthrough]];
    default:  // Other types added by healthD that are unknown here yet.
      NOTREACHED() << "Encountered unhandled or unknown crash type "
                   << crash_type;
  }
}

MetricData FatalCrashEventsObserver::FillFatalCrashTelemetry(
    const CrashEventInfoPtr& info,
    std::optional<std::string> event_based_log_upload_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  MetricData metric_data;
  metric_data.mutable_event_data()->set_type(MetricEventType::FATAL_CRASH);

  FatalCrashTelemetry& data =
      *metric_data.mutable_telemetry_data()->mutable_fatal_crash_telemetry();

  data.set_type(GetFatalCrashTelemetryCrashType(info->crash_type));

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

  if (event_based_log_upload_id.has_value()) {
    *data.mutable_event_based_log_id() = event_based_log_upload_id.value();
  }

  return metric_data;
}

void FatalCrashEventsObserver::AddEventLogObserver(
    FatalCrashEventLogObserver* observer) {
  event_log_observers_.AddObserver(observer);
}

void FatalCrashEventsObserver::RemoveEventLogObserver(
    FatalCrashEventLogObserver* observer) {
  event_log_observers_.RemoveObserver(observer);
}

std::optional<std::string>
FatalCrashEventsObserver::NotifyFatalCrashEventLog() {
  // We won't generate an upload ID if there's no observers to trigger the log
  // upload. Observers will only exist if device log upload policy is enabled.
  if (event_log_observers_.empty()) {
    return std::nullopt;
  }
  std::string upload_id = policy::EventBasedLogUploader::GenerateUploadId();
  for (auto& observer : event_log_observers_) {
    observer.OnFatalCrashEvent(upload_id);
  }
  return upload_id;
}

}  // namespace reporting
