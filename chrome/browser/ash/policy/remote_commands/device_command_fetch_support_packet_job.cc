// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/remote_commands/device_command_fetch_support_packet_job.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/check_is_test.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/metrics/histogram_functions.h"
#include "base/sequence_checker.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/syslog_logging.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/core/device_cloud_policy_manager_ash.h"
#include "chrome/browser/ash/policy/remote_commands/crd/crd_remote_command_utils.h"
#include "chrome/browser/ash/policy/uploading/system_log_uploader.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part_ash.h"
#include "chrome/browser/policy/messaging_layer/proto/synced/log_upload_event.pb.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/support_tool/data_collection_module.pb.h"
#include "chrome/browser/support_tool/data_collector.h"
#include "chrome/browser/support_tool/support_tool_util.h"
#include "chrome/browser/ui/webui/support_tool/support_tool_ui_utils.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/components/kiosk/kiosk_utils.h"
#include "components/feedback/redaction_tool/pii_types.h"
#include "components/policy/core/common/remote_commands/remote_command_job.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/reporting/client/report_queue.h"
#include "components/reporting/client/report_queue_configuration.h"
#include "components/reporting/client/report_queue_factory.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/util/status.h"

using enterprise_management::FetchSupportPacketResultCode;
using enterprise_management::FetchSupportPacketResultNote;
using enterprise_management::UserSessionType;

namespace {

static const base::FilePath* g_target_directory_for_testing = nullptr;

// The directory that the support packets will be stored.
constexpr char kTargetDir[] = "/var/spool/support";
constexpr char kFilenamePrefix[] = "admin-generated";

// JSON keys used in the remote command payload.
constexpr char kSupportPacketDetailsKey[] = "supportPacketDetails";
constexpr char kIssueCaseIdKey[] = "issueCaseId";
constexpr char kIssueDescriptionKey[] = "issueDescription";
constexpr char kRequestedDataCollectorsKey[] = "requestedDataCollectors";
constexpr char kRequestedPiiTypesKey[] = "requestedPiiTypes";

// JSON keys and values used for creating the upload metadata to File Storage
// Server (go/crosman_fss_action#scotty-upload-agent).
constexpr char kFileTypeKey[] = "File-Type";
constexpr char kSupportFileType[] = "support_file";
constexpr char kCommandIdKey[] = "Command-ID";
constexpr char kContentTypeJson[] = "application/json";
constexpr char kFilenameKey[] = "Filename";

// JSON keys that are used in command result payload.
constexpr char kResultKey[] = "result";
constexpr char kNotesKey[] = "notes";

std::set<support_tool::DataCollectorType> GetDataCollectorTypes(
    const base::Value::List& requested_data_collectors) {
  std::set<support_tool::DataCollectorType> data_collectors;
  for (const auto& data_collector_value : requested_data_collectors) {
    if (!support_tool::DataCollectorType_IsValid(
            data_collector_value.GetInt())) {
      continue;
    }
    data_collectors.emplace(static_cast<support_tool::DataCollectorType>(
        data_collector_value.GetInt()));
  }
  return data_collectors;
}

redaction::PIIType GetPiiTypeFromProtoEnum(support_tool::PiiType pii_type) {
  switch (pii_type) {
    case support_tool::PiiType::ANDROID_APP_STORAGE_PATH:
      return redaction::PIIType::kAndroidAppStoragePath;
    case support_tool::PiiType::EMAIL:
      return redaction::PIIType::kEmail;
    case support_tool::PiiType::GAIA_ID:
      return redaction::PIIType::kGaiaID;
    case support_tool::PiiType::IPP_ADDRESS:
      return redaction::PIIType::kIPPAddress;
    case support_tool::PiiType::IP_ADDRESS:
      return redaction::PIIType::kIPAddress;
    case support_tool::PiiType::CELLULAR_LOCATION_INFO:
      return redaction::PIIType::kCellularLocationInfo;
    case support_tool::PiiType::MAC_ADDRESS:
      return redaction::PIIType::kMACAddress;
    case support_tool::PiiType::UI_HIEARCHY_WINDOW_TITLE:
      return redaction::PIIType::kUIHierarchyWindowTitles;
    case support_tool::PiiType::URL:
      return redaction::PIIType::kURL;
    case support_tool::PiiType::SERIAL:
      return redaction::PIIType::kSerial;
    case support_tool::PiiType::SSID:
      return redaction::PIIType::kSSID;
    case support_tool::PiiType::STABLE_IDENTIFIER:
      return redaction::PIIType::kStableIdentifier;
    case support_tool::PiiType::VOLUME_LABEL:
      return redaction::PIIType::kVolumeLabel;
    case support_tool::PiiType::EAP:
      return redaction::PIIType::kEAP;
    default:
      return redaction::PIIType::kNone;
  }
}

std::set<redaction::PIIType> GetPiiTypes(
    const base::Value::List& requested_pii_types) {
  std::set<redaction::PIIType> pii_types;
  for (const auto& pii_type_value : requested_pii_types) {
    if (!support_tool::PiiType_IsValid(pii_type_value.GetInt())) {
      continue;
    }
    pii_types.emplace(GetPiiTypeFromProtoEnum(
        static_cast<support_tool::PiiType>(pii_type_value.GetInt())));
  }
  return pii_types;
}

// Returns the upload_parameters string for LogUploadEvent. This will be used as
// request metadata for the log upload request to the File Storage Server.
// Contains File-Type, Command-ID and Filename fields.
// The details of metadata format can be found in
// go/crosman_fss_action#scotty-upload-agent.
std::string GetUploadParameters(
    const base::FilePath& filename,
    policy::RemoteCommandJob::UniqueIDType command_id) {
  auto upload_parameters_dict =
      base::Value::Dict()
          .Set(kFilenameKey, filename.BaseName().value())
          .Set(kCommandIdKey, base::NumberToString(command_id))
          .Set(kFileTypeKey, kSupportFileType);
  std::string json;
  base::JSONWriter::Write(upload_parameters_dict, &json);
  return base::StringPrintf("%s\n%s", json.c_str(), kContentTypeJson);
}

std::string GetCommandResultPayload(
    FetchSupportPacketResultCode result_code,
    const std::set<FetchSupportPacketResultNote>& notes) {
  base::Value::Dict json;
  json.Set(kResultKey, static_cast<int>(result_code));
  if (!notes.empty()) {
    base::Value::List notes_list;
    for (const auto& note : notes) {
      notes_list.Append(static_cast<int>(note));
    }
    json.Set(kNotesKey, std::move(notes_list));
  }
  std::string result_payload;
  base::JSONWriter::Write(json, &result_payload);
  return result_payload;
}

}  // namespace

namespace policy {

const char kFetchSupportPacketFailureHistogramName[] =
    "Enterprise.DeviceRemoteCommand.FetchSupportPacket.Failure";

// static
void DeviceCommandFetchSupportPacketJob::SetTargetDirForTesting(
    const base::FilePath* target_dir) {
  CHECK_IS_TEST();
  g_target_directory_for_testing = target_dir;
}

DeviceCommandFetchSupportPacketJob::DeviceCommandFetchSupportPacketJob() =
    default;

DeviceCommandFetchSupportPacketJob::~DeviceCommandFetchSupportPacketJob() =
    default;

SupportPacketDetails::SupportPacketDetails() = default;
SupportPacketDetails::~SupportPacketDetails() = default;

enterprise_management::RemoteCommand_Type
DeviceCommandFetchSupportPacketJob::GetType() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return enterprise_management::RemoteCommand_Type_FETCH_SUPPORT_PACKET;
}

const base::FilePath DeviceCommandFetchSupportPacketJob::GetTargetDir() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (g_target_directory_for_testing) {
    CHECK_IS_TEST();
    return *g_target_directory_for_testing;
  }
  return base::FilePath(kTargetDir);
}

bool DeviceCommandFetchSupportPacketJob::ParseCommandPayload(
    const std::string& command_payload) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  bool parse_success = ParseCommandPayloadImpl(command_payload);
  if (!parse_success) {
    base::UmaHistogramEnumeration(
        kFetchSupportPacketFailureHistogramName,
        EnterpriseFetchSupportPacketFailureType::kFailedOnWrongCommandPayload);
    SYSLOG(ERROR) << "Can't parse command payload for FETCH_SUPPORT_PACKET "
                     "command. Payload is: "
                  << command_payload;
  }
  return parse_success;
}

bool DeviceCommandFetchSupportPacketJob::ParseCommandPayloadImpl(
    const std::string& command_payload) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::optional<base::Value> value = base::JSONReader::Read(command_payload);
  if (!value.has_value() || !value->is_dict()) {
    return false;
  }

  const base::Value::Dict& dict = value->GetDict();
  const base::Value::Dict* details_dict =
      dict.FindDict(kSupportPacketDetailsKey);
  if (!details_dict) {
    return false;
  }

  const std::string* case_id = details_dict->FindString(kIssueCaseIdKey);
  support_packet_details_.issue_case_id = case_id ? *case_id : std::string();

  const std::string* description =
      details_dict->FindString(kIssueDescriptionKey);
  support_packet_details_.issue_description =
      description ? *description : std::string();

  const base::Value::List* requested_data_collectors =
      details_dict->FindList(kRequestedDataCollectorsKey);
  if (!requested_data_collectors) {
    return false;
  }
  support_packet_details_.requested_data_collectors =
      GetDataCollectorTypes(*requested_data_collectors);
  // Requested data collectors can't be empty.
  if (support_packet_details_.requested_data_collectors.empty()) {
    return false;
  }

  const base::Value::List* requested_pii_types =
      details_dict->FindList(kRequestedPiiTypesKey);
  if (requested_pii_types) {
    support_packet_details_.requested_pii_types =
        GetPiiTypes(*requested_pii_types);
  }

  return true;
}

bool DeviceCommandFetchSupportPacketJob::IsCommandEnabled() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  bool log_upload_enabled;
  if (!ash::CrosSettings::IsInitialized() ||
      !ash::CrosSettings::Get()->GetBoolean(ash::kSystemLogUploadEnabled,
                                            &log_upload_enabled)) {
    return false;
  }
  return log_upload_enabled;
}

void DeviceCommandFetchSupportPacketJob::RunImpl(
    CallbackWithResult result_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  result_callback_ = std::move(result_callback);
  // Check if the command is enabled for the user.
  if (!IsCommandEnabled()) {
    base::UmaHistogramEnumeration(kFetchSupportPacketFailureHistogramName,
                                  EnterpriseFetchSupportPacketFailureType::
                                      kFailedOnCommandEnabledForUserCheck);
    SYSLOG(ERROR)
        << "FETCH_SUPPORT_PACKET command is not enabled for the device.";
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            std::move(result_callback_), ResultType::kFailure,
            GetCommandResultPayload(
                FetchSupportPacketResultCode::FAILURE_COMMAND_NOT_ENABLED,
                notes_)));
    return;
  }

  StartJobExecution();
}

bool DeviceCommandFetchSupportPacketJob::IsPiiAllowed() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  switch (current_session_type_) {
    case UserSessionType::AUTO_LAUNCHED_KIOSK_SESSION:
    case UserSessionType::MANUALLY_LAUNCHED_KIOSK_SESSION:
    case UserSessionType::AFFILIATED_USER_SESSION:
      return true;

    case UserSessionType::UNAFFILIATED_USER_SESSION:
    case UserSessionType::MANAGED_GUEST_SESSION:
    case UserSessionType::GUEST_SESSION:
    case UserSessionType::NO_SESSION:
    case UserSessionType::USER_SESSION_TYPE_UNKNOWN:
      return false;
  }
  return false;
}

void DeviceCommandFetchSupportPacketJob::StartJobExecution() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  current_session_type_ = GetCurrentUserSessionType();

  // Get sign-in profile on sign-in screen.
  // TODO: b/252962974 - Remove the dependency to sign-in profile and use
  // nullptr when user profile isn't created yet.
  Profile* profile = current_session_type_ == UserSessionType::NO_SESSION
                         ? Profile::FromBrowserContext(
                               CHECK_DEREF(ash::BrowserContextHelper::Get())
                                   .GetSigninBrowserContext())
                         : ProfileManager::GetActiveUserProfile();
  // Initialize SupportToolHandler with the requested details.
  support_tool_handler_ = GetSupportToolHandler(
      support_packet_details_.issue_case_id,
      // Leave the email address empty since data
      // collection is triggered by the admin remotely.
      /*email_address=*/std::string(),
      support_packet_details_.issue_description, std::nullopt, profile,
      support_packet_details_.requested_data_collectors);

  // Start data collection.
  support_tool_handler_->CollectSupportData(
      base::BindOnce(&DeviceCommandFetchSupportPacketJob::OnDataCollected,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DeviceCommandFetchSupportPacketJob::OnDataCollected(
    const PIIMap& detected_pii,
    std::set<SupportToolError> errors) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Log the errors for information. We don't return any error for the command
  // job, we just continue the operation with as much data as we could collect.
  if (!errors.empty()) {
    SYSLOG(ERROR) << "Got errors when collecting data for FETCH_SUPPORT_PACKET "
                     "device command: "
                  << SupportToolErrorsToString(errors);
  }

  base::FilePath target_file = GetFilepathToExport(
      GetTargetDir(), kFilenamePrefix, support_tool_handler_->GetCaseId(),
      base::Time::Now());

  std::set<redaction::PIIType> pii_types =
      support_packet_details_.requested_pii_types;
  if (!pii_types.empty() && !IsPiiAllowed()) {
    notes_.emplace(FetchSupportPacketResultNote::WARNING_PII_NOT_ALLOWED);
    pii_types.clear();
  }

  support_tool_handler_->ExportCollectedData(
      pii_types, target_file,
      base::BindOnce(&DeviceCommandFetchSupportPacketJob::OnDataExported,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DeviceCommandFetchSupportPacketJob::OnDataExported(
    base::FilePath exported_path,
    std::set<SupportToolError> errors) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const auto export_error =
      base::ranges::find(errors, SupportToolErrorCode::kDataExportError,
                         &SupportToolError::error_code);

  if (export_error != errors.end()) {
    base::UmaHistogramEnumeration(kFetchSupportPacketFailureHistogramName,
                                  EnterpriseFetchSupportPacketFailureType::
                                      kFailedOnExportingSupportPacket);
    SYSLOG(ERROR) << "The device couldn't export the collected data "
                     "into local storage: "
                  << export_error->error_message;
    std::move(result_callback_)
        .Run(ResultType::kFailure,
             GetCommandResultPayload(
                 FetchSupportPacketResultCode::FAILURE_EXPORTING_FILE, notes_));
    return;
  }

  exported_path_ = exported_path;

  ::reporting::SourceInfo source_info;
  source_info.set_source(::reporting::SourceInfo::ASH);
  ::reporting::ReportQueueFactory::Create(
      ::reporting::ReportQueueConfiguration::Create(
          {.event_type = ::reporting::EventType::kDevice,
           .destination = ::reporting::Destination::LOG_UPLOAD})
          .SetSourceInfo(std::move(source_info)),
      base::BindOnce(&DeviceCommandFetchSupportPacketJob::OnReportQueueCreated,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DeviceCommandFetchSupportPacketJob::OnReportQueueCreated(
    std::unique_ptr<reporting::ReportQueue> report_queue) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SYSLOG(INFO) << "ReportQueue is created for LogUploadEvent.";
  report_queue_ = std::move(report_queue);
  EnqueueEvent();
}

void DeviceCommandFetchSupportPacketJob::EnqueueEvent() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto log_upload_event = std::make_unique<ash::reporting::LogUploadEvent>();
  log_upload_event->mutable_upload_settings()->set_origin_path(
      exported_path_.value());
  log_upload_event->mutable_upload_settings()->set_upload_parameters(
      GetUploadParameters(exported_path_, unique_id()));

  auto* command_details = log_upload_event->mutable_remote_command_details();
  command_details->set_command_id(unique_id());
  command_details->set_command_result_payload(GetCommandResultPayload(
      FetchSupportPacketResultCode::FETCH_SUPPORT_PACKET_RESULT_SUCCESS,
      notes_));

  report_queue_->Enqueue(
      std::move(log_upload_event), reporting::Priority::SLOW_BATCH,
      base::BindOnce(&DeviceCommandFetchSupportPacketJob::OnEventEnqueued,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DeviceCommandFetchSupportPacketJob::OnEventEnqueued(
    reporting::Status status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (status.ok()) {
    base::UmaHistogramEnumeration(
        kFetchSupportPacketFailureHistogramName,
        EnterpriseFetchSupportPacketFailureType::kNoFailure);
    SYSLOG(INFO) << "FETCH_SUPPORT_PACKET command job has successfully "
                    "finished execution.";
    // Command result will only be send to DMServer if the command execution is
    // finished (with success or failure). It won't be sent when command is only
    // acked. That's why we omit result payload here. FETCH_SUPPORT_PACKET
    // command execution will be counted as finished only when LogUploadEvent is
    // uploaded to Reporting server.
    std::move(result_callback_).Run(ResultType::kAcked, std::nullopt);
    return;
  }

  base::UmaHistogramEnumeration(
      kFetchSupportPacketFailureHistogramName,
      EnterpriseFetchSupportPacketFailureType::kFailedOnEnqueueingEvent);
  SYSLOG(ERROR) << "Couldn't enqueue event to reporting queue:  "
                << status.error_message();
  std::move(result_callback_)
      .Run(ResultType::kFailure,
           GetCommandResultPayload(
               FetchSupportPacketResultCode::FAILURE_REPORTING_PIPELINE,
               notes_));
}

}  // namespace policy
