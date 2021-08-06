// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/remote_commands/crd_host_delegate.h"

#include <string>

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/ash/policy/remote_commands/crd_connection_observer.h"
#include "chrome/browser/ash/policy/remote_commands/crd_logging.h"
#include "chrome/browser/browser_process.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/api/messaging/native_message_host.h"
#include "google_apis/gaia/gaia_constants.h"
#include "remoting/host/it2me/it2me_constants.h"
#include "remoting/host/it2me/it2me_native_messaging_host_chromeos.h"
#include "remoting/protocol/errors.h"

namespace policy {

namespace {

class DefaultNativeMessageHostFactory
    : public CRDHostDelegate::NativeMessageHostFactory {
 public:
  DefaultNativeMessageHostFactory() = default;
  DefaultNativeMessageHostFactory(const DefaultNativeMessageHostFactory&) =
      delete;
  DefaultNativeMessageHostFactory& operator=(
      const DefaultNativeMessageHostFactory&) = delete;
  ~DefaultNativeMessageHostFactory() override = default;

  // CRDHostDelegate::NativeMessageHostFactory implementation:
  std::unique_ptr<extensions::NativeMessageHost> CreateNativeMessageHostHost()
      override {
    return remoting::CreateIt2MeNativeMessagingHostForChromeOS(
        content::GetIOThreadTaskRunner({}), content::GetUIThreadTaskRunner({}),
        g_browser_process->policy_service());
  }
};

bool IsConnectionRejected(const std::string& disconnect_reason) {
  return (disconnect_reason ==
          remoting::protocol::ErrorCodeToString(
              remoting::protocol::ErrorCode::SESSION_REJECTED));
}

std::string FindStringKeyOrDefault(
    const base::Value& dictionary,
    const std::string& key_name,
    const std::string& default_value = "<missing-key>") {
  const std::string* result = dictionary.FindStringKey(key_name);
  return result ? *result : default_value;
}

}  // namespace

CRDHostDelegate::CRDHostDelegate()
    : CRDHostDelegate(std::make_unique<DefaultNativeMessageHostFactory>()) {}

CRDHostDelegate::CRDHostDelegate(
    std::unique_ptr<NativeMessageHostFactory> factory)
    : factory_(std::move(factory)) {
  DCHECK(factory_);
}

CRDHostDelegate::~CRDHostDelegate() = default;

void CRDHostDelegate::AddConnectionObserver(CrdConnectionObserver* observer) {
  connection_observers_.AddObserver(observer);
}

bool CRDHostDelegate::HasActiveSession() const {
  return host_ != nullptr;
}

void CRDHostDelegate::TerminateSession(base::OnceClosure callback) {
  DoShutdownHost();
  std::move(callback).Run();
}

void CRDHostDelegate::StartCRDHostAndGetCode(
    const SessionParameters& parameters,
    DeviceCommandStartCRDSessionJob::AccessCodeCallback success_callback,
    DeviceCommandStartCRDSessionJob::ErrorCallback error_callback) {
  DCHECK(!host_);
  DCHECK(!code_success_callback_);
  DCHECK(!error_callback_);

  // Store all parameters for future connect call.
  base::Value connect_params(base::Value::Type::DICTIONARY);

  connect_params.SetStringKey(remoting::kUserName, parameters.user_name);
  connect_params.SetStringKey(remoting::kAuthServiceWithToken,
                              "oauth2:" + parameters.oauth_token);
  connect_params.SetBoolKey(remoting::kTerminateUponInput,
                            parameters.terminate_upon_input);
  connect_params.SetBoolKey(remoting::kIsEnterpriseAdminUser, true);
  // Note both |kSuppressUserDialogs| and |kSuppressNotifications| are
  // controlled by |show_confirmation_dialog|.
  connect_params.SetBoolKey(remoting::kSuppressUserDialogs,
                            !parameters.show_confirmation_dialog);
  connect_params.SetBoolKey(remoting::kSuppressNotifications,
                            !parameters.show_confirmation_dialog);

  connect_params_ = std::move(connect_params);

  remote_connected_ = false;
  command_awaiting_crd_access_code_ = true;

  code_success_callback_ = std::move(success_callback);
  error_callback_ = std::move(error_callback);

  // TODO(antrim): set up watchdog timer (reasonable cutoff).
  host_ = factory_->CreateNativeMessageHostHost();
  host_->Start(this);

  base::Value params(base::Value::Type::DICTIONARY);
  SendMessageToHost(remoting::kHelloMessage, params);
}

void CRDHostDelegate::PostMessageFromNativeHost(
    const std::string& message_string) {
  CRD_DVLOG(1) << "Received message from CRD host: " << message_string;

  absl::optional<base::Value> message = base::JSONReader::Read(message_string);
  if (!message) {
    OnProtocolBroken("Message is invalid JSON");
    return;
  }

  if (!message->is_dict()) {
    OnProtocolBroken("Message is not a dictionary");
    return;
  }

  const std::string* type = message->FindStringKey(remoting::kMessageType);
  if (!type) {
    OnProtocolBroken("Message without type");
    return;
  }

  HandleNativeHostMessage(*type, *message);
}

void CRDHostDelegate::HandleNativeHostMessage(const std::string& type,
                                              const base::Value& message) {
  if (type == remoting::kHelloResponse) {
    OnHelloResponse();
  } else if (type == remoting::kConnectResponse) {
    //  Ok, just ignore.
  } else if (type == remoting::kDisconnectResponse) {
    OnDisconnectResponse();
  } else if (type == remoting::kErrorMessage) {
    OnConnectionError(
        FindStringKeyOrDefault(message, remoting::kErrorMessageCode));
  } else if (type == remoting::kHostStateChangedMessage) {
    //  Handle CRD host state changes
    const std::string* state = message.FindStringKey(remoting::kState);
    if (!state) {
      OnProtocolBroken("No state in message");
      return;
    }
    HandleNativeHostStateChangeMessage(*state, message);
  } else {
    CRD_LOG(WARNING) << "Unknown message type: " << type;
  }
}

void CRDHostDelegate::HandleNativeHostStateChangeMessage(
    const std::string& state,
    const base::Value& message) {
  if (state == remoting::kHostStateReceivedAccessCode) {
    OnStateReceivedAccessCode(message);
  } else if (state == remoting::kHostStateConnected) {
    OnStateRemoteConnected(message);
  } else if (state == remoting::kHostStateDisconnected) {
    OnStateRemoteDisconnected(
        FindStringKeyOrDefault(message, remoting::kDisconnectReason));
  } else if (state == remoting::kHostStateDomainError) {
    OnConnectionError(state);
  } else if (state == remoting::kHostStateStarting ||
             state == remoting::kHostStateRequestedAccessCode) {
    //  Just ignore these states.
  } else {
    CRD_LOG(WARNING) << "Unhandled state: " << state;
  }
}

void CRDHostDelegate::OnHelloResponse() {
  // Host is initialized, start connection.
  SendMessageToHost(remoting::kConnectMessage, connect_params_);
}

void CRDHostDelegate::OnDisconnectResponse() {
  // Should happen only when remoting session finished and we
  // have requested host to shut down, or when we have got second auth code
  // without receiving connection.
  DCHECK(!command_awaiting_crd_access_code_);
  DCHECK(!remote_connected_);
  ShutdownHost();
}

void CRDHostDelegate::OnConnectionError(const std::string& error_reason) {
  // Notify callback if command is still running.
  if (command_awaiting_crd_access_code_) {
    command_awaiting_crd_access_code_ = false;
    std::move(error_callback_)
        .Run(DeviceCommandStartCRDSessionJob::FAILURE_CRD_HOST_ERROR,
             "CRD Connection Error: " + error_reason);
    code_success_callback_.Reset();
  }
  // Shut down host, if any.
  ShutdownHost();
}

void CRDHostDelegate::OnStateRemoteConnected(const base::Value& message) {
  for (auto& observer : connection_observers_)
    observer.OnConnectionEstablished();

  remote_connected_ = true;

  // TODO(antrim): set up watchdog timer (session duration).
  const std::string* client = message.FindStringKey(remoting::kClient);
  if (client)
    CRD_DVLOG(1) << "Remote connection by " << *client;
}

void CRDHostDelegate::OnStateRemoteDisconnected(
    const std::string& disconnect_reason) {
  // There could be a connection attempt that was not successful, we will
  // receive "disconnected" message without actually receiving "connected".
  if (!remote_connected_) {
    CRD_DVLOG(1) << "Connection failed with reason: " << disconnect_reason;

    if (IsConnectionRejected(disconnect_reason)) {
      for (auto& observer : connection_observers_)
        observer.OnConnectionRejected();
    }
    return;
  }
  remote_connected_ = false;

  CRD_DVLOG(1) << "Remote host has disconnected";
  // Remote has disconnected, time to send "disconnect" that would result
  // in shutting down the host.
  base::Value params(base::Value::Type::DICTIONARY);
  SendMessageToHost(remoting::kDisconnectMessage, params);
}

void CRDHostDelegate::OnStateReceivedAccessCode(const base::Value& message) {
  if (!command_awaiting_crd_access_code_) {
    if (!remote_connected_) {
      // We have already sent the access code back to the server which
      // initiated this CRD session through a remote command, and we can not
      // send a new access code. Assuming that the old access code is no
      // longer valid, we can only terminate the current CRD session.
      base::Value params(base::Value::Type::DICTIONARY);
      SendMessageToHost(remoting::kDisconnectMessage, params);
    }
    return;
  }

  const std::string* access_code = message.FindStringKey(remoting::kAccessCode);
  absl::optional<int> code_lifetime =
      message.FindIntKey(remoting::kAccessCodeLifetime);
  if (!access_code || !code_lifetime) {
    OnProtocolBroken("Can not obtain access code");
    return;
  }

  CRD_DVLOG(1) << "Got access code";
  // TODO(antrim): set up watchdog timer (access code lifetime).
  command_awaiting_crd_access_code_ = false;
  std::move(code_success_callback_).Run(*access_code);
  error_callback_.Reset();
}

void CRDHostDelegate::CloseChannel(const std::string& error_message) {
  CRD_LOG(ERROR) << "CRD Host closed channel" << error_message;
  command_awaiting_crd_access_code_ = false;

  if (error_callback_) {
    std::move(error_callback_)
        .Run(DeviceCommandStartCRDSessionJob::FAILURE_CRD_HOST_ERROR,
             error_message);
  }
  code_success_callback_.Reset();
  ShutdownHost();
}

void CRDHostDelegate::SendMessageToHost(const std::string& type,
                                        base::Value& params) {
  CRD_DVLOG(1) << "Sending message of type '" << type << "' to CRD host.";
  std::string message_json;
  params.SetKey(remoting::kMessageType, base::Value(type));
  base::JSONWriter::Write(params, &message_json);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&CRDHostDelegate::DoSendMessage,
                                weak_factory_.GetWeakPtr(), message_json));
}

void CRDHostDelegate::DoSendMessage(const std::string& json) {
  if (!host_)
    return;
  host_->OnMessage(json);
}

void CRDHostDelegate::OnProtocolBroken(const std::string& message) {
  CRD_LOG(ERROR) << "Error communicating with CRD Host : " << message;
  command_awaiting_crd_access_code_ = false;

  std::move(error_callback_)
      .Run(DeviceCommandStartCRDSessionJob::FAILURE_CRD_HOST_ERROR, message);
  code_success_callback_.Reset();
  ShutdownHost();
}

void CRDHostDelegate::ShutdownHost() {
  if (!host_)
    return;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&CRDHostDelegate::DoShutdownHost,
                                weak_factory_.GetWeakPtr()));
}

void CRDHostDelegate::DoShutdownHost() {
  host_.reset();
}

}  // namespace policy
