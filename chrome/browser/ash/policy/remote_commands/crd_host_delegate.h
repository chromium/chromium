// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_CRD_HOST_DELEGATE_H_
#define CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_CRD_HOST_DELEGATE_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/ash/policy/remote_commands/device_command_start_crd_session_job.h"
#include "extensions/browser/api/messaging/native_message_host.h"

namespace policy {

class CrdConnectionObserver;

// Delegate that will start a session with the CRD native host.
// Will keep the session alive and active as long as this class lives.
// Deleting this class object will forcefully interrupt the active CRD session.
class CRDHostDelegate : public DeviceCommandStartCRDSessionJob::Delegate,
                        public extensions::NativeMessageHost::Client {
 public:
  class NativeMessageHostFactory {
   public:
    virtual ~NativeMessageHostFactory() = default;

    virtual std::unique_ptr<extensions::NativeMessageHost>
    CreateNativeMessageHostHost() = 0;
  };

  CRDHostDelegate();
  explicit CRDHostDelegate(std::unique_ptr<NativeMessageHostFactory> factory);
  ~CRDHostDelegate() override;

  // DeviceCommandStartCRDSessionJob::Delegate:
  bool HasActiveSession() const override;
  void TerminateSession(base::OnceClosure callback) override;
  void StartCRDHostAndGetCode(
      const SessionParameters& parameters,
      DeviceCommandStartCRDSessionJob::AccessCodeCallback success_callback,
      DeviceCommandStartCRDSessionJob::ErrorCallback error_callback) override;

  // Set a connection observer, which will be informed when either:
  //   - The CRD connection is successfully established, or
  //   - The CRD connection failed to be established.
  void AddConnectionObserver(CrdConnectionObserver* observer);

 private:
  // extensions::NativeMessageHost::Client:
  // Invoked when native host sends a message
  void PostMessageFromNativeHost(const std::string& message) override;
  void CloseChannel(const std::string& error_message) override;

  void HandleNativeHostMessage(const std::string& type,
                               const base::Value& message);
  void HandleNativeHostStateChangeMessage(const std::string& state,
                                          const base::Value& message);

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

  void OnConnectionError(const std::string& error_reason);
  void OnStateRemoteConnected(const base::Value& message);
  void OnStateRemoteDisconnected(const std::string& disconnect_reason);
  void OnStateReceivedAccessCode(const base::Value& message);

  std::unique_ptr<NativeMessageHostFactory> factory_;

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

  // Owned by the same |DeviceCommandsFactoryChromeOS| as |this|.
  base::ObserverList<CrdConnectionObserver> connection_observers_;

  base::WeakPtrFactory<CRDHostDelegate> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CRDHostDelegate);
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_CRD_HOST_DELEGATE_H_
