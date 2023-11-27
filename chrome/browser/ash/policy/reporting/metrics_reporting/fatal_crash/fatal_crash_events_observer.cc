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
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/fatal_crash/fatal_crash_events_observer_reported_local_id_manager.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/fatal_crash/fatal_crash_events_observer_save_file_paths_provider.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/fatal_crash/fatal_crash_events_observer_settings_for_test.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/fatal_crash/fatal_crash_events_observer_uploaded_crash_info_manager.h"
#include "chromeos/ash/services/cros_healthd/public/cpp/service_connection.h"
#include "components/reporting/proto/synced/metric_data.pb.h"
#include "components/user_manager/user_type.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace reporting {

using ::ash::cros_healthd::mojom::CrashEventInfo;
using ::ash::cros_healthd::mojom::CrashEventInfoPtr;
using ::ash::cros_healthd::mojom::EventInfoPtr;

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
    : FatalCrashEventsObserver(DefaultSaveFilePathsProvider::Get(),
                               /*reported_local_id_io_task_runner=*/nullptr,
                               /*uploaded_crash_info_io_task_runner=*/nullptr) {
}

FatalCrashEventsObserver::FatalCrashEventsObserver(
    const SaveFilePathsProviderInterface& save_file_paths_provider,
    scoped_refptr<base::SequencedTaskRunner> reported_local_id_io_task_runner,
    scoped_refptr<base::SequencedTaskRunner> uploaded_crash_info_io_task_runner)
    : MojoServiceEventsObserverBase<ash::cros_healthd::mojom::EventObserver>(
          this),
      reported_local_id_manager_{ReportedLocalIdManager::Create(
          save_file_paths_provider.GetReportedLocalIdSaveFilePath(),
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
          save_file_paths_provider.GetUploadedCrashInfoSaveFilePath(),
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

  auto& crash_event_info = info->get_crash_event_info();
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

  MetricData metric_data = FillFatalCrashTelemetry(crash_event_info);
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

  MetricData metric_data = FillFatalCrashTelemetry(crash_event_info);
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

MetricData FatalCrashEventsObserver::FillFatalCrashTelemetry(
    const CrashEventInfoPtr& info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  MetricData metric_data;
  metric_data.mutable_event_data()->set_type(MetricEventType::FATAL_CRASH);

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
}  // namespace reporting
