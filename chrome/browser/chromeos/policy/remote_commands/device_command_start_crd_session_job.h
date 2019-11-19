// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_REMOTE_COMMANDS_DEVICE_COMMAND_START_CRD_SESSION_JOB_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_REMOTE_COMMANDS_DEVICE_COMMAND_START_CRD_SESSION_JOB_H_

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/policy/core/common/remote_commands/remote_command_job.h"

namespace policy {

// Remote command that would start Chrome Remote Desktop host and return auth
// code. This command is usable only for devices running Kiosk sessions.
class DeviceCommandStartCRDSessionJob : public RemoteCommandJob {
 public:
  enum ResultCode {
    // Successfully obtained access code.
    SUCCESS = 0,

    // Failed as required services are not launched on the device.
    FAILURE_SERVICES_NOT_READY = 1,

    // Failed as device is not running in Kiosk mode.
    FAILURE_NOT_A_KIOSK = 2,

    // Failed as device is currently in use and no interruptUser flag is set.
    FAILURE_NOT_IDLE = 3,

    // Failed as we could not get OAuth token for whatever reason.
    FAILURE_NO_OAUTH_TOKEN = 4,

    // Failed as we could not get ICE configuration for whatever reason.
    FAILURE_NO_ICE_CONFIG = 5,

    // Failure during attempt to start CRD host and obtain CRD token.
    FAILURE_CRD_HOST_ERROR = 6,
  };

  using OAuthTokenCallback = base::OnceCallback<void(const std::string&)>;
  using AccessCodeCallback = base::OnceCallback<void(const std::string&)>;
  using ICEConfigCallback = base::OnceCallback<void(base::Value)>;
  using ErrorCallback =
      base::OnceCallback<void(ResultCode, const std::string&)>;

  // A delegate interface used by DeviceCommandStartCRDSessionJob to retrieve
  // its dependencies.
  class Delegate {
   public:
    virtual ~Delegate() {}

    // Check if there exists an active CRD session.
    virtual bool HasActiveSession() const = 0;

    // Run |callback| once active CRD session is terminated.
    virtual void TerminateSession(base::OnceClosure callback) = 0;

    // Check if required system services are ready.
    virtual bool AreServicesReady() const = 0;

    // Check if device is running an auto-launched Kiosk.
    virtual bool IsRunningKiosk() const = 0;

    // Return current user idleness period.
    virtual base::TimeDelta GetIdlenessPeriod() const = 0;

    // Attempts to get OAuth token for CRD Host.
    virtual void FetchOAuthToken(OAuthTokenCallback success_callback,
                                 ErrorCallback error_callback) = 0;

    // Attempts to get ICE configuration for CRD Host.
    virtual void FetchICEConfig(const std::string& oauth_token,
                                ICEConfigCallback success_callback,
                                ErrorCallback error_callback) = 0;

    // Attempts to start CRD host and get Auth Code.
    virtual void StartCRDHostAndGetCode(const std::string& oauth_token,
                                        base::Value ice_config,
                                        bool terminate_upon_input,
                                        AccessCodeCallback success_callback,
                                        ErrorCallback error_callback) = 0;
  };

  explicit DeviceCommandStartCRDSessionJob(Delegate* crd_host_delegate);
  ~DeviceCommandStartCRDSessionJob() override;

  // RemoteCommandJob:
  enterprise_management::RemoteCommand_Type GetType() const override;

 protected:
  // RemoteCommandJob:
  bool ParseCommandPayload(const std::string& command_payload) override;
  void RunImpl(CallbackWithResult succeeded_callback,
               CallbackWithResult failed_callback) override;
  void TerminateImpl() override;

 private:
  class ResultPayload;

  // Finishes command with error code and optional message.
  void FinishWithError(ResultCode result_code, const std::string& message);

  void OnOAuthTokenReceived(const std::string& token);
  void OnICEConfigReceived(base::Value ice_config);
  void OnAccessCodeReceived(const std::string& access_code);

  // The callback that will be called when the access code was successfully
  // obtained.
  CallbackWithResult succeeded_callback_;

  // The callback that will be called when this command failed.
  CallbackWithResult failed_callback_;

  // -- Command parameters --

  // Defines whether connection attempt to active user should succeed or fail.
  base::TimeDelta idleness_cutoff_;

  // Defines if CRD session should be terminated upon any input event from local
  // user.
  bool terminate_upon_input_ = false;

  std::string oauth_token_;
  base::Value ice_config_;

  // The Delegate is used to interact with chrome services and CRD host.
  // Owned by DeviceCommandsFactoryChromeOS.
  Delegate* delegate_;

  bool terminate_session_attemtpted_;

  base::WeakPtrFactory<DeviceCommandStartCRDSessionJob> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DeviceCommandStartCRDSessionJob);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_REMOTE_COMMANDS_DEVICE_COMMAND_START_CRD_SESSION_JOB_H_
