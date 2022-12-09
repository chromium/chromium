// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_DEVICE_COMMAND_START_CRD_SESSION_JOB_H_
#define CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_DEVICE_COMMAND_START_CRD_SESSION_JOB_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/policy/core/common/remote_commands/remote_command_job.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace policy {

// Remote command that would start Chrome Remote Desktop host and return auth
// code. This command is usable only for devices running Kiosk sessions, for
// Affiliated Users and for Managed Guest Sessions.
class DeviceCommandStartCrdSessionJob : public RemoteCommandJob {
 public:
  // This enum can't be renumbered because it's logged to UMA and because its
  // values must match the values in the server side
  // `DeviceCommandUtil::ClientResultCode` enum.
  enum class ResultCode {
    // Successfully obtained access code.
    SUCCESS = 0,

    // Failed as required services are not launched on the device.
    // deprecated FAILURE_SERVICES_NOT_READY = 1,

    // Failure as the current user type does not support remotely starting CRD.
    FAILURE_UNSUPPORTED_USER_TYPE = 2,

    // Failed as device is currently in use and no interruptUser flag is set.
    FAILURE_NOT_IDLE = 3,

    // Failed as we could not get OAuth token for whatever reason.
    FAILURE_NO_OAUTH_TOKEN = 4,

    // Failed as we could not get ICE configuration for whatever reason.
    // deprecated FAILURE_NO_ICE_CONFIG = 5,

    // Failure during attempt to start CRD host and obtain CRD token.
    FAILURE_CRD_HOST_ERROR = 6,

    // Failure to start a curtained session as we're not in a managed
    // environment.
    FAILURE_UNMANAGED_ENVIRONMENT = 7,

    kMaxValue = FAILURE_UNMANAGED_ENVIRONMENT
  };

  using OAuthTokenCallback = base::OnceCallback<void(const std::string&)>;
  using AccessCodeCallback = base::OnceCallback<void(const std::string&)>;
  using ErrorCallback =
      base::OnceCallback<void(ResultCode, const std::string&)>;

  // Delegate that will start a session with the CRD native host.
  class Delegate {
   public:
    // Session parameters used to start the CRD host.
    struct SessionParameters {
      std::string oauth_token = "";
      std::string user_name = "";
      bool terminate_upon_input = false;
      bool show_confirmation_dialog = false;
      bool curtain_local_user_session = false;
    };

    virtual ~Delegate() = default;

    // Check if there exists an active CRD session.
    virtual bool HasActiveSession() const = 0;

    // Run |callback| once active CRD session is terminated.
    virtual void TerminateSession(base::OnceClosure callback) = 0;

    // Attempts to start CRD host and get Auth Code.
    virtual void StartCrdHostAndGetCode(const SessionParameters& parameters,
                                        AccessCodeCallback success_callback,
                                        ErrorCallback error_callback) = 0;
  };

  explicit DeviceCommandStartCrdSessionJob(Delegate* crd_host_delegate);
  ~DeviceCommandStartCrdSessionJob() override;

  DeviceCommandStartCrdSessionJob(const DeviceCommandStartCrdSessionJob&) =
      delete;
  DeviceCommandStartCrdSessionJob& operator=(
      const DeviceCommandStartCrdSessionJob&) = delete;

  // RemoteCommandJob:
  enterprise_management::RemoteCommand_Type GetType() const override;

  // Set a Fake OAuth token that will be used once the next time we need to
  // fetch an oauth token.
  void SetOAuthTokenForTest(const std::string& token);

  // This enum can't be renumbered because it is logged to UMA.
  enum class UmaSessionType {
    kAutoLaunchedKiosk = 0,
    kAffiliatedUser = 1,
    kManagedGuestSession = 2,
    kManuallyLaunchedKiosk = 3,
    kMaxValue = kManuallyLaunchedKiosk
  };

 protected:
  // RemoteCommandJob:
  bool ParseCommandPayload(const std::string& command_payload) override;
  void RunImpl(CallbackWithResult succeeded_callback,
               CallbackWithResult failed_callback) override;
  void TerminateImpl() override;

 private:
  class OAuthTokenFetcher;

  void CheckManagedNetworkASync(base::OnceClosure on_success);
  void FetchOAuthTokenASync(OAuthTokenCallback on_success);
  void StartCrdHostAndGetCode(const std::string& token);
  void FinishWithSuccess(const std::string& access_code);
  // Finishes command with error code and optional message.
  void FinishWithError(ResultCode result_code, const std::string& message);
  void FinishWithNotIdleError();

  bool UserTypeSupportsCrd() const;
  UmaSessionType GetUmaSessionType() const;
  bool IsDeviceIdle() const;

  std::string GetRobotAccountUserName() const;
  bool ShouldShowConfirmationDialog() const;
  bool ShouldTerminateUponInput() const;

  ErrorCallback GetErrorCallback();

  std::unique_ptr<OAuthTokenFetcher> oauth_token_fetcher_;

  // The callback that will be called when the access code was successfully
  // obtained.
  CallbackWithResult succeeded_callback_;

  // The callback that will be called when this command failed.
  CallbackWithResult failed_callback_;

  // -- Command parameters --

  // Defines whether connection attempt to active user should succeed or fail.
  base::TimeDelta idleness_cutoff_;

  // True if the admin has confirmed that they want to start the CRD session,
  // while a user is currently using the device.
  bool acked_user_presence_ = false;

  // True if the admin requested a curtained remote access session.
  bool curtain_local_user_session_ = false;

  // -- End of command parameters --

  // Fake OAuth token that will be used once the next time we need to fetch an
  // oauth token.
  absl::optional<std::string> oauth_token_for_test_;

  // The Delegate is used to interact with chrome services and CRD host.
  // Owned by DeviceCommandsFactoryAsh.
  Delegate* const delegate_;

  bool terminate_session_attempted_ = false;

  base::WeakPtrFactory<DeviceCommandStartCrdSessionJob> weak_factory_{this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_DEVICE_COMMAND_START_CRD_SESSION_JOB_H_
