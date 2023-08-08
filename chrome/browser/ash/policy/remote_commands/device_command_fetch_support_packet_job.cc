// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/remote_commands/device_command_fetch_support_packet_job.h"

#include <algorithm>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_is_test.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece_forward.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/syslog_logging.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "chrome/browser/policy/messaging_layer/proto/synced/log_upload_event.pb.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/support_tool/data_collection_module.pb.h"
#include "chrome/browser/support_tool/data_collector.h"
#include "chrome/browser/support_tool/support_tool_util.h"
#include "chrome/browser/ui/webui/support_tool/support_tool_ui_utils.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "components/feedback/redaction_tool/pii_types.h"
#include "components/policy/core/common/remote_commands/remote_command_job.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/reporting/client/report_queue.h"
#include "components/reporting/client/report_queue_configuration.h"
#include "components/reporting/client/report_queue_factory.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/util/status.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace {

// The directory that the support packets will be stored.
constexpr char kTargetDir[] = "/var/spool/support";
constexpr char kFilenamePrefix[] = "admin-generated";

// JSON keys used in the remote command payload.
constexpr char kSupportPacketDetailsKey[] = "supportPacketDetails";
constexpr char kIssueCaseIdKey[] = "issueCaseId";
constexpr char kIssueDescriptionKey[] = "issueDescription";
constexpr char kRequestedDataCollectorsKey[] = "requestedDataCollectors";
constexpr char kRequestedPiiTypesKey[] = "requestedPiiTypes";
constexpr char kRequesterId[] = "requesterMetadata";

// JSON keys and values used for creating the upload metadata to File Storage
// Server (go/crosman_fss_action#scotty-upload-agent).
constexpr char kFileTypeKey[] = "File-Type";
constexpr char kSupportFileType[] = "support_file";
constexpr char kCommandIdKey[] = "Command-ID";
constexpr char kContentTypeJson[] = "application/json";
constexpr char kFilenameKey[] = "Filename";

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

std::string ErrorsToString(const std::set<SupportToolError>& errors) {
  std::vector<base::StringPiece> error_messages;
  error_messages.reserve(errors.size());
  for (const auto& error : errors) {
    error_messages.push_back(error.error_message);
  }
  return base::JoinString(error_messages, ", ");
}

// Returns the upload_parameters string for LogUploadEvent. This will be used as
// request metadata for the log upload request to the File Storage Server.
// Contains File-Type, Command-ID and Filename fields.
// The details of metadata format can be found in
// go/crosman_fss_action#scotty-upload-agent.
std::string GetUploadParameters(
    const base::FilePath& filename,
    policy::RemoteCommandJob::UniqueIDType command_id) {
  base::Value::Dict upload_parameters_dict;
  upload_parameters_dict.Set(kFilenameKey, filename.BaseName().value().c_str());
  upload_parameters_dict.Set(kCommandIdKey, base::NumberToString(command_id));
  upload_parameters_dict.Set(kFileTypeKey, kSupportFileType);
  std::string json;
  base::JSONWriter::Write(upload_parameters_dict, &json);
  return base::StringPrintf("%s\n%s", json.c_str(), kContentTypeJson);
}

}  // namespace

namespace policy {

const char kCommandNotEnabledForUserMessage[] =
    "FETCH_SUPPORT_PACKET command is not enabled for this user type.";

const char kFetchSupportPacketFailureHistogramName[] =
    "Enterprise.DeviceRemoteCommand.FetchSupportPacket.Failure";

DeviceCommandFetchSupportPacketJob::DeviceCommandFetchSupportPacketJob()
    : target_dir_(kTargetDir) {}

DeviceCommandFetchSupportPacketJob::~DeviceCommandFetchSupportPacketJob() {
  // Clean-up `login_waiter_`.
  login_waiter_.reset();
}

SupportPacketDetails::SupportPacketDetails() = default;
SupportPacketDetails::~SupportPacketDetails() = default;

DeviceCommandFetchSupportPacketJob::LoginWaiter::LoginWaiter() {
  CHECK(ash::LoginState::IsInitialized());
  ash::LoginState::Get()->AddObserver(this);
}
DeviceCommandFetchSupportPacketJob::LoginWaiter::~LoginWaiter() {
  ash::LoginState::Get()->RemoveObserver(this);
}

void DeviceCommandFetchSupportPacketJob::LoginWaiter::WaitForLogin(
    base::OnceClosure on_user_logged_in_callback) {
  if (ash::LoginState::Get()->IsUserLoggedIn()) {
    std::move(on_user_logged_in_callback).Run();
    return;
  }
  SYSLOG(INFO) << "Waiting for a user to login for executing "
                  "FETCH_SUPPORT_PACKET command.";
  on_user_logged_in_callback_ = std::move(on_user_logged_in_callback);
}

void DeviceCommandFetchSupportPacketJob::LoginWaiter::LoggedInStateChanged() {
  if (!on_user_logged_in_callback_) {
    return;
  }
  std::move(on_user_logged_in_callback_).Run();
}

enterprise_management::RemoteCommand_Type
DeviceCommandFetchSupportPacketJob::GetType() const {
  return enterprise_management::RemoteCommand_Type_FETCH_SUPPORT_PACKET;
}

void DeviceCommandFetchSupportPacketJob::SetReportQueueForTesting(
    std::unique_ptr<reporting::ReportQueue> report_queue) {
  CHECK_IS_TEST();
  report_queue_ = std::move(report_queue);
}

bool DeviceCommandFetchSupportPacketJob::ParseCommandPayload(
    const std::string& command_payload) {
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
  absl::optional<base::Value> value = base::JSONReader::Read(command_payload);
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

  const std::string* requester_metadata =
      details_dict->FindString(kRequesterId);
  support_packet_details_.requester_metadata =
      requester_metadata ? *requester_metadata : std::string();

  return true;
}

// static
bool DeviceCommandFetchSupportPacketJob::CommandEnabledForUser() {
  return ash::LoginState::Get()->IsKioskSession();
}

void DeviceCommandFetchSupportPacketJob::RunImpl(
    CallbackWithResult result_callback) {
  result_callback_ = std::move(result_callback);

  // Wait for a user to login to start the execution.
  CHECK(!login_waiter_.has_value());
  login_waiter_.emplace();
  login_waiter_->WaitForLogin(
      base::BindOnce(&DeviceCommandFetchSupportPacketJob::OnUserLoggedIn,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DeviceCommandFetchSupportPacketJob::OnUserLoggedIn() {
  // Clean up the `login_waiter_`.
  login_waiter_.reset();

  StartJobExecution();
}

void DeviceCommandFetchSupportPacketJob::StartJobExecution() {
  // Check if the command is enabled for the user type.
  if (!CommandEnabledForUser()) {
    base::UmaHistogramEnumeration(kFetchSupportPacketFailureHistogramName,
                                  EnterpriseFetchSupportPacketFailureType::
                                      kFailedOnCommandEnabledForUserCheck);
    SYSLOG(ERROR) << kCommandNotEnabledForUserMessage;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(result_callback_), ResultType::kFailure,
                       kCommandNotEnabledForUserMessage));
    return;
  }

  // Initialize SupportToolHandler with the requested details.
  support_tool_handler_ =
      GetSupportToolHandler(support_packet_details_.issue_case_id,
                            // Leave the email address empty since data
                            // collection is triggered by the admin remotely.
                            /*email_address=*/std::string(),
                            support_packet_details_.issue_description,
                            ProfileManager::GetActiveUserProfile(),
                            support_packet_details_.requested_data_collectors);

  // Start data collection.
  support_tool_handler_->CollectSupportData(
      base::BindOnce(&DeviceCommandFetchSupportPacketJob::OnDataCollected,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DeviceCommandFetchSupportPacketJob::OnDataCollected(
    const PIIMap& detected_pii,
    std::set<SupportToolError> errors) {
  // Log the errors for information. We don't return any error for the command
  // job, we just continue the operation with as much data as we could collect.
  if (!errors.empty()) {
    SYSLOG(ERROR) << "Got errors when collecting data for FETCH_SUPPORT_PACKET "
                     "device command: "
                  << ErrorsToString(errors);
  }

  base::FilePath target_file = GetFilepathToExport(
      target_dir_, kFilenamePrefix, support_tool_handler_->GetCaseId(),
      base::Time::Now());

  support_tool_handler_->ExportCollectedData(
      support_packet_details_.requested_pii_types, target_file,
      base::BindOnce(&DeviceCommandFetchSupportPacketJob::OnDataExported,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DeviceCommandFetchSupportPacketJob::OnDataExported(
    base::FilePath exported_path,
    std::set<SupportToolError> errors) {
  const auto export_error =
      base::ranges::find(errors, SupportToolErrorCode::kDataExportError,
                         &SupportToolError::error_code);

  if (export_error != errors.end()) {
    base::UmaHistogramEnumeration(kFetchSupportPacketFailureHistogramName,
                                  EnterpriseFetchSupportPacketFailureType::
                                      kFailedOnExportingSupportPacket);
    std::string error_message =
        base::StrCat({"The device couldn't export the collected data "
                      "into local storage: ",
                      export_error->error_message});
    SYSLOG(ERROR) << error_message;
    std::move(result_callback_).Run(ResultType::kFailure, error_message);
    return;
  }

  exported_path_ = exported_path;

  // No need to create a `report_queue_` if it is already initialized. Since the
  // DeviceCommandFetchSupportPacketJob instance will be created per command,
  // `report_queue_` will only be already initialized for tests by
  // `SetReportQueueForTesting()` function.
  if (report_queue_) {
    EnqueueEvent();
    return;
  }

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
  SYSLOG(INFO) << "ReportQueue is created for LogUploadEvent.";
  report_queue_ = std::move(report_queue);
  EnqueueEvent();
}

void DeviceCommandFetchSupportPacketJob::EnqueueEvent() {
  auto log_upload_event = std::make_unique<ash::reporting::LogUploadEvent>();
  log_upload_event->mutable_upload_settings()->set_origin_path(
      exported_path_.value());
  log_upload_event->mutable_upload_settings()->set_upload_parameters(
      GetUploadParameters(exported_path_, unique_id()));
  log_upload_event->set_command_id(unique_id());
  report_queue_->Enqueue(
      std::move(log_upload_event), reporting::Priority::SLOW_BATCH,
      base::BindOnce(&DeviceCommandFetchSupportPacketJob::OnEventEnqueued,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DeviceCommandFetchSupportPacketJob::OnEventEnqueued(
    reporting::Status status) {
  if (status.ok()) {
    base::UmaHistogramEnumeration(
        kFetchSupportPacketFailureHistogramName,
        EnterpriseFetchSupportPacketFailureType::kNoFailure);
    SYSLOG(INFO) << "FETCH_SUPPORT_PACKET command job has successfully "
                    "finished execution.";
    std::move(result_callback_).Run(ResultType::kAcked, absl::nullopt);
    return;
  }

  std::string error_message = base::StrCat(
      {"Couldn't enqueue event to reporting queue:  ", status.error_message()});

  base::UmaHistogramEnumeration(
      kFetchSupportPacketFailureHistogramName,
      EnterpriseFetchSupportPacketFailureType::kFailedOnEnqueueingEvent);
  SYSLOG(ERROR) << error_message;
  std::move(result_callback_).Run(ResultType::kFailure, error_message);
}

}  // namespace policy
