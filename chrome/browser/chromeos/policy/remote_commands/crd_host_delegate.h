// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_REMOTE_COMMANDS_CRD_HOST_DELEGATE_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_REMOTE_COMMANDS_CRD_HOST_DELEGATE_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/chromeos/policy/remote_commands/device_command_start_crd_session_job.h"
#include "extensions/browser/api/messaging/native_message_host.h"

class Profile;

namespace policy {

// An implementation of the |DeviceCommandStartCRDSessionJob::Delegate|.
class CRDHostDelegate : public DeviceCommandStartCRDSessionJob::Delegate,
                        public extensions::NativeMessageHost::Client {
 public:
  CRDHostDelegate();
  ~CRDHostDelegate() override;

 private:
  class OAuthTokenFetcher;

  // DeviceCommandStartCRDSessionJob::Delegate:
  bool HasActiveSession() const override;
  void TerminateSession(base::OnceClosure callback) override;
  bool AreServicesReady() const override;
  bool IsRunningKiosk() const override;
  base::TimeDelta GetIdlenessPeriod() const override;
  void FetchOAuthToken(
      DeviceCommandStartCRDSessionJob::OAuthTokenCallback success_callback,
      DeviceCommandStartCRDSessionJob::ErrorCallback error_callback) override;
  void StartCRDHostAndGetCode(
      const std::string& oauth_token,
      bool terminate_upon_input,
      DeviceCommandStartCRDSessionJob::AccessCodeCallback success_callback,
      DeviceCommandStartCRDSessionJob::ErrorCallback error_callback) override;

  // extensions::NativeMessageHost::Client:
  // Invoked when native host sends a message
  void PostMessageFromNativeHost(const std::string& message) override;
  void CloseChannel(const std::string& error_message) override;

  // Sends message to host in separate task.
  void SendMessageToHost(const std::string& type, base::Value& params);
  // Actually sends message to host.
  void DoSendMessage(const std::string& json);
  void OnProtocolBroken(const std::string& message);
  // Shuts down host in a separate task.
  void ShutdownHost();
  // Actually shuts down a host.
  void DoShutdownHost();

  // Handlers for messages from host
  void OnHelloResponse();
  void OnDisconnectResponse();

  void OnStateError(std::string error_state, base::Value& message);
  void OnStateRemoteConnected(base::Value& message);
  void OnStateRemoteDisconnected();
  void OnStateReceivedAccessCode(base::Value& message);

  Profile* GetKioskProfile() const;

  std::unique_ptr<OAuthTokenFetcher> oauth_token_fetcher_;

  DeviceCommandStartCRDSessionJob::AccessCodeCallback code_success_callback_;
  DeviceCommandStartCRDSessionJob::ErrorCallback error_callback_;

  std::unique_ptr<extensions::NativeMessageHost> host_;

  // Filled structure with parameters for "connect" message.
  base::Value connect_params_;

  // Determines actions when receiving messages from CRD host,
  // if command is still running (no error / access code), then
  // callbacks have to be called.
  bool command_awaiting_crd_access_code_;
  // True if remote session was established.
  bool remote_connected_;

  base::WeakPtrFactory<CRDHostDelegate> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CRDHostDelegate);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_REMOTE_COMMANDS_CRD_HOST_DELEGATE_H_
