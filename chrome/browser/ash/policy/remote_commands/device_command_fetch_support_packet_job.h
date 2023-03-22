// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_DEVICE_COMMAND_FETCH_SUPPORT_PACKET_JOB_H_
#define CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_DEVICE_COMMAND_FETCH_SUPPORT_PACKET_JOB_H_

#include <memory>
#include <set>
#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/support_tool/data_collection_module.pb.h"
#include "chrome/browser/support_tool/support_tool_handler.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "components/feedback/redaction_tool/pii_types.h"
#include "components/policy/core/common/remote_commands/remote_command_job.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace policy {

// The error message that's send as command result payload when the command is
// not enabled for user's type.
extern const char kCommandNotEnabledForUserMessage[];

// SupportPacketDetails will contain the details of the data collection that's
// requested by the remote command's payload. Command payload contains the
// JSON-encoded version of SupportPacketDetails proto message in
// chrome/browser/support_tool/data_collection_module.proto and this struct is
// based on the contents of the proto.
// TODO(iremuguz): We may remove SupportPacketDetails proto message altogether
// and reference to the proto in server-side here since we're not using it
// anymore.
struct SupportPacketDetails {
  std::string issue_case_id;
  std::string issue_description;
  std::set<support_tool::DataCollectorType> requested_data_collectors;
  std::set<redaction::PIIType> requested_pii_types;
  std::string requester_metadata;

  SupportPacketDetails();
  ~SupportPacketDetails();
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

  // Convenience functions for testing. `/var/spool/support` path shouldn't be
  // used in unit tests so it should be replaced.
  base::FilePath GetExportedFilepathForTesting() { return exported_path_; }
  void SetTargetDirForTesting(base::FilePath target_dir) {
    target_dir_ = target_dir;
  }

 protected:
  // RemoteCommandJob:
  void RunImpl(CallbackWithResult result_callback) override;
  bool ParseCommandPayload(const std::string& command_payload) override;

 private:
  // LoginWaiter observes the LoginState and run the callback once the user is
  // logged in or if a user is already logged in. It's expected to be called
  // once with `WaitForLogin()` function and should be cleaned-up once it
  // triggers the callback upon user login.
  class LoginWaiter : public ash::LoginState::Observer {
   public:
    LoginWaiter();
    LoginWaiter(const LoginWaiter&) = delete;
    LoginWaiter& operator=(const LoginWaiter&) = delete;
    ~LoginWaiter() override;

    void WaitForLogin(base::OnceClosure on_user_logged_in_callback);

   private:
    // ash::LoginState::Observer
    void LoggedInStateChanged() override;

    base::OnceClosure on_user_logged_in_callback_;
  };

  // Checks if the command should be enabled for the user type. Currently it's
  // only enabled for logged-in users that are on kiosk session.
  static bool CommandEnabledForUser();

  // Parses the `command_payload` into `support_packet_details_`. Returns false
  // if the `command_payload` is not in the format that we expect.
  bool ParseCommandPayloadImpl(const std::string& command_payload);

  // Is called when we wait for a user to login to start command execution.
  // Cleans up `login_waiter_` and starts command execution.
  void OnUserLoggedIn();

  // Verifies that the command is enabled for the user and starts the data
  // collection.
  void StartJobExecution();

  void OnDataCollected(const PIIMap& detected_pii,
                       std::set<SupportToolError> errros);

  void OnDataExported(base::FilePath exported_path,
                      std::set<SupportToolError> errors);

  // The directory to export the generated support packet.
  base::FilePath target_dir_;
  // The filepath of the exported support packet. It will be a file within
  // `target_dir_`.
  base::FilePath exported_path_;
  // The details of requested support packet. Contains details like data
  // collectors, PII types, case ID etc.
  SupportPacketDetails support_packet_details_;
  // The callback to run when the execution of RemoteCommandJob has finished.
  CallbackWithResult result_callback_;
  std::unique_ptr<SupportToolHandler> support_tool_handler_;
  // `login_waiter_` will wait for a user to login if the command was called on
  // login screen.
  absl::optional<LoginWaiter> login_waiter_;
  base::WeakPtrFactory<DeviceCommandFetchSupportPacketJob> weak_ptr_factory_{
      this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_DEVICE_COMMAND_FETCH_SUPPORT_PACKET_JOB_H_
