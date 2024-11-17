// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/event_based_logs/event_based_log_uploader.h"

#include <algorithm>
#include <optional>
#include <set>
#include <string>

#include "base/check_deref.h"
#include "base/check_is_test.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/json/json_writer.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/ash/policy/reporting/event_based_logs/event_based_log_utils.h"
#include "chrome/browser/policy/messaging_layer/proto/synced/log_upload_event.pb.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/support_tool/data_collection_module.pb.h"
#include "chrome/browser/support_tool/data_collector.h"
#include "chrome/browser/support_tool/support_tool_util.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/reporting/client/report_queue_factory.h"
#include "components/reporting/util/status.h"
#include "components/user_manager/user_manager.h"
#include "record.pb.h"

namespace {

static const base::FilePath* g_target_directory_for_testing = nullptr;

// JSON keys and values used for creating the upload metadata to File Storage
// Server (go/crosman_fss_action#scotty-upload-agent).
constexpr char kFileTypeKey[] = "File-Type";
constexpr char kEventBasedLogFileType[] = "event_based_log_file";
constexpr char kContentTypeJson[] = "application/json";
constexpr char kFilenameKey[] = "Filename";
constexpr char kUploadedEventTypeKey[] = "Uploaded-Event-Type";
constexpr char kUploadIdKey[] = "Upload-ID";

// The directory that the log files will be stored.
constexpr char kTargetDir[] = "/var/spool/support";
constexpr char kEventBasedLogFileNamePrefixFormat[] = "event-based_%s";

std::string GetEventBasedLogFilenamePrefix(
    ash::reporting::TriggerEventType event_type) {
  return base::StringPrintf(
      kEventBasedLogFileNamePrefixFormat,
      ash::reporting::TriggerEventType_Name(event_type).c_str());
}

// Returns the upload_parameters string for LogUploadEvent. This will be used as
// request metadata for the log upload request to the File Storage Server.
// Contains File-Type and Filename fields.
// The details of metadata format can be found in
// go/crosman_fss_action#scotty-upload-agent.
std::string GetUploadParameters(
    const base::FilePath& filename,
    const ash::reporting::TriggerEventType& trigger_event_type,
    const std::string& upload_id) {
  auto upload_parameters_dict =
      base::Value::Dict()
          .Set(kFilenameKey, filename.BaseName().value())
          // Safe to cast enum value to int.
          .Set(kUploadedEventTypeKey, static_cast<int>(trigger_event_type))
          .Set(kFileTypeKey, kEventBasedLogFileType)
          .Set(kUploadIdKey, upload_id);
  std::string json;
  base::JSONWriter::Write(upload_parameters_dict, &json);
  return base::StringPrintf("%s\n%s", json.c_str(), kContentTypeJson);
}

}  // namespace

namespace policy {

EventBasedLogUploader::EventBasedLogUploader() = default;
EventBasedLogUploader::~EventBasedLogUploader() = default;

EventBasedLogUploaderImpl::EventBasedLogUploaderImpl() = default;
EventBasedLogUploaderImpl::~EventBasedLogUploaderImpl() = default;

// static
void EventBasedLogUploaderImpl::SetTargetDirForTesting(
    const base::FilePath* target_dir) {
  CHECK_IS_TEST();
  g_target_directory_for_testing = target_dir;
}

const base::FilePath EventBasedLogUploaderImpl::GetTargetDir() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (g_target_directory_for_testing) {
    CHECK_IS_TEST();
    return *g_target_directory_for_testing;
  }
  return base::FilePath(kTargetDir);
}

void EventBasedLogUploaderImpl::UploadEventBasedLogs(
    std::set<support_tool::DataCollectorType> data_collectors,
    ash::reporting::TriggerEventType event_type,
    std::optional<std::string> upload_id,
    UploadCallback on_upload_completed) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // If `upload_id_` is already set, it means there's an ongoing upload. After
  // each upload is enqueued `upload_id_` is reset.
  if (upload_id_.has_value()) {
    std::move(on_upload_completed)
        .Run(reporting::Status(
            reporting::error::ALREADY_EXISTS,
            "Ongoing upload already exists for the event type."));
    return;
  }
  event_type_ = event_type;
  on_upload_completed_ = std::move(on_upload_completed);

  Profile* profile = !user_manager::UserManager::Get()->IsUserLoggedIn()
                         ? Profile::FromBrowserContext(
                               CHECK_DEREF(ash::BrowserContextHelper::Get())
                                   .GetSigninBrowserContext())
                         : ProfileManager::GetActiveUserProfile();

  upload_id_ = upload_id.value_or(GenerateEventBasedLogUploadId());
  // Initialize SupportToolHandler.
  support_tool_handler_ =
      GetSupportToolHandler(/*issue_case_id=*/std::string(),
                            /*email_address=*/std::string(),
                            /*issue_description=*/std::string(), upload_id_,
                            profile, data_collectors);

  // Start data collection.
  support_tool_handler_->CollectSupportData(
      base::BindOnce(&EventBasedLogUploaderImpl::OnDataCollected,
                     weak_ptr_factory_.GetWeakPtr()));
}

void EventBasedLogUploaderImpl::OnDataCollected(
    const PIIMap& detected_pii,
    std::set<SupportToolError> errors) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Log the errors for information. We just continue the operation with as much
  // data as we could collect.
  if (!errors.empty()) {
    LOG(ERROR) << "Got errors for event based log collection: "
               << SupportToolErrorsToString(errors);
  }

  base::FilePath target_file = GetFilepathToExport(
      GetTargetDir(), GetEventBasedLogFilenamePrefix(event_type_),
      /*case_id=*/std::string(), base::Time::Now());

  support_tool_handler_->ExportCollectedData(
      // All PII must be removed from collected data.
      /*pii_types_to_keep=*/{}, target_file,
      base::BindOnce(&EventBasedLogUploaderImpl::OnDataExported,
                     weak_ptr_factory_.GetWeakPtr()));
}

void EventBasedLogUploaderImpl::OnDataExported(
    base::FilePath exported_path,
    std::set<SupportToolError> errors) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const auto export_error =
      base::ranges::find(errors, SupportToolErrorCode::kDataExportError,
                         &SupportToolError::error_code);

  if (export_error != errors.end()) {
    LOG(ERROR) << "The device couldn't export the collected data "
                  "into local storage: "
               << export_error->error_message;
    upload_id_ = std::nullopt;
    std::move(on_upload_completed_)
        .Run(reporting::Status(reporting::error::INTERNAL,
                               export_error->error_message));
    return;
  }

  // `report_queue_` can already be created if event based logs were uploaded
  // before. We will keep using the same `report_queue_`.
  if (report_queue_) {
    EnqueueEvent(exported_path);
    return;
  }

  ::reporting::SourceInfo source_info;
  source_info.set_source(::reporting::SourceInfo::ASH);
  ::reporting::ReportQueueFactory::Create(
      ::reporting::ReportQueueConfiguration::Create(
          {.event_type = ::reporting::EventType::kDevice,
           .destination = ::reporting::Destination::LOG_UPLOAD})
          .SetSourceInfo(std::move(source_info)),
      base::BindOnce(&EventBasedLogUploaderImpl::OnReportQueueCreated,
                     weak_ptr_factory_.GetWeakPtr(), exported_path));
}

void EventBasedLogUploaderImpl::OnReportQueueCreated(
    base::FilePath exported_path,
    std::unique_ptr<reporting::ReportQueue> report_queue) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(0) << "ReportQueue is created for event based log collection.";
  report_queue_ = std::move(report_queue);
  EnqueueEvent(exported_path);
}

void EventBasedLogUploaderImpl::EnqueueEvent(base::FilePath exported_path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto log_upload_event = std::make_unique<ash::reporting::LogUploadEvent>();
  log_upload_event->mutable_upload_settings()->set_origin_path(
      exported_path.value());
  log_upload_event->mutable_upload_settings()->set_upload_parameters(
      GetUploadParameters(exported_path, event_type_, upload_id_.value()));

  auto* trigger_event_details =
      log_upload_event->mutable_trigger_event_details();
  trigger_event_details->set_trigger_event_type(event_type_);
  trigger_event_details->set_log_upload_id(upload_id_.value());

  report_queue_->Enqueue(
      std::move(log_upload_event), reporting::Priority::SLOW_BATCH,
      base::BindOnce(&EventBasedLogUploaderImpl::OnEventEnqueued,
                     weak_ptr_factory_.GetWeakPtr()));
}

void EventBasedLogUploaderImpl::OnEventEnqueued(reporting::Status status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // We reset the `upload_id_` after enqueuing the event to reporting service.
  upload_id_ = std::nullopt;
  std::move(on_upload_completed_).Run(status);
}

}  // namespace policy
