// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_DEVICE_COMMAND_FETCH_SUPPORT_PACKET_JOB_H_
#define CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_DEVICE_COMMAND_FETCH_SUPPORT_PACKET_JOB_H_

#include <memory>
#include <set>
#include <string>

#include "base/check_is_test.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "chrome/browser/support_tool/data_collection_module.pb.h"
#include "chrome/browser/support_tool/support_tool_handler.h"
#include "components/feedback/redaction_tool/pii_types.h"
#include "components/policy/core/common/remote_commands/remote_command_job.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/reporting/client/report_queue.h"
#include "components/reporting/util/status.h"

namespace policy {

// DeviceCommandFetchSupportPacketJob will emit
// EnterpriseFetchSupportPacketFailure enums to this UMA histogram.
extern const char kFetchSupportPacketFailureHistogramName[];

// SupportPacketDetails will contain the details of the data collection that's
// requested by the remote command's payload. Command payload contains the
// JSON-encoded version of SupportPacketDetails proto message in
// http://google3/chrome/cros/dpanel/data/devices/proto/requests.proto.
struct SupportPacketDetails {
  std::string issue_case_id;
  std::string issue_description;
  std::set<support_tool::DataCollectorType> requested_data_collectors;
  std::set<redaction::PIIType> requested_pii_types;

  SupportPacketDetails();
  ~SupportPacketDetails();
};

// This enum is emitted to UMA
// `Enterprise.DeviceRemoteCommand.FetchSupportPacket.Failure` and can't be
// renumerated. Please update `EnterpriseFetchSupportPacketFailureType` values
// in tools/metrics/histograms/enums.xml when you add a new value to here.
enum class EnterpriseFetchSupportPacketFailureType {
  kNoFailure = 0,
  kFailedOnWrongCommandPayload = 1,
  kFailedOnCommandEnabledForUserCheck = 2,
  kFailedOnExportingSupportPacket = 3,
  kFailedOnEnqueueingEvent = 4,
  kMaxValue = kFailedOnEnqueueingEvent,
};

class DeviceCommandFetchSupportPacketJob : public RemoteCommandJob {
 public:
  DeviceCommandFetchSupportPacketJob();

  DeviceCommandFetchSupportPacketJob(
      const DeviceCommandFetchSupportPacketJob&) = delete;
  DeviceCommandFetchSupportPacketJob& operator=(
      const DeviceCommandFetchSupportPacketJob&) = delete;

  ~DeviceCommandFetchSupportPacketJob() override;

  // RemoteCommandJob:
  enterprise_management::RemoteCommand_Type GetType() const override;

  // Convenience function for testing. `/var/spool/support` path can't be
  // used in unit/browser tests so it should be replaced by a temporary
  // directory. The caller test is responsible for cleaning this path up after
  // testing is done and calling `SetTargetDirForTesting(nullptr)` to reset the
  // target dir.
  static void SetTargetDirForTesting(const base::FilePath* target_dir);

 protected:
  // RemoteCommandJob:
  void RunImpl(CallbackWithResult result_callback) override;
  bool ParseCommandPayload(const std::string& command_payload) override;

 private:
  // Returns /var/spool/support. A temporary directory that's set by
  // `SetTargetDirForTesting()` will be used for testing.
  const base::FilePath GetTargetDir();

  // Checks if the command should be enabled. Returns true if the
  // SystemLogEnabled policy is enabled.
  bool IsCommandEnabled() const;

  // Checks if the requested PII can be kept in the collected logs. PII is only
  // allowed on kiosk sessions and affiliated user sessions. For all other
  // sessions types (e.g. unaffiliated user session, MGS, guest session and
  // sign-in screen), PII is not allowed to be included.
  bool IsPiiAllowed() const;

  // Parses the `command_payload` into `support_packet_details_`. Returns false
  // if the `command_payload` is not in the format that we expect.
  bool ParseCommandPayloadImpl(const std::string& command_payload);

  // Verifies that the command is enabled for the user and starts the data
  // collection.
  void StartJobExecution();

  void OnDataCollected(const PIIMap& detected_pii,
                       std::set<SupportToolError> errros);

  void OnDataExported(base::FilePath exported_path,
                      std::set<SupportToolError> errors);

  void OnReportQueueCreated(
      std::unique_ptr<reporting::ReportQueue> report_queue);

  void EnqueueEvent();

  void OnEventEnqueued(reporting::Status status);

  SEQUENCE_CHECKER(sequence_checker_);
  // The filepath of the exported support packet. It will be a file within
  // GetTargetDir().
  base::FilePath exported_path_;
  // The details of requested support packet. Contains details like data
  // collectors, PII types, case ID etc.
  SupportPacketDetails support_packet_details_;
  // These are the notes that will be included in the result payload when the
  // execution is completed.
  std::set<enterprise_management::FetchSupportPacketResultNote> notes_;
  // The callback to run when the execution of RemoteCommandJob has finished.
  CallbackWithResult result_callback_;
  // Session type when job execution starts in `StartJobExecution()`.
  // `current_session_type_` won't be updated later even if a user logs in and
  // PII handling will be done according to this session type since the logs are
  // collected from this current session when job execution first starts.
  enterprise_management::UserSessionType current_session_type_;
  std::unique_ptr<SupportToolHandler> support_tool_handler_;
  std::unique_ptr<reporting::ReportQueue> report_queue_;
  base::WeakPtrFactory<DeviceCommandFetchSupportPacketJob> weak_ptr_factory_{
      this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_DEVICE_COMMAND_FETCH_SUPPORT_PACKET_JOB_H_
