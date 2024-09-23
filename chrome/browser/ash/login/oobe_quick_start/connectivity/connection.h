// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_CONNECTIVITY_CONNECTION_H_
#define CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_CONNECTIVITY_CONNECTION_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/account_transfer_client_data.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/fido_assertion_info.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/session_context.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/target_device_connection_broker.h"
#include "chromeos/ash/components/nearby/common/connections_manager/nearby_connection.h"
#include "chromeos/ash/components/quick_start/quick_start_metrics.h"
#include "chromeos/ash/components/quick_start/quick_start_response_type.h"
#include "chromeos/ash/components/quick_start/types.h"
#include "chromeos/ash/services/nearby/public/mojom/quick_start_decoder.mojom.h"
#include "chromeos/ash/services/nearby/public/mojom/quick_start_decoder_types.mojom-shared.h"
#include "chromeos/ash/services/nearby/public/mojom/quick_start_decoder_types.mojom.h"
#include "components/cbor/values.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "mojo/public/cpp/bindings/struct_ptr.h"
#include "url/origin.h"

namespace ash::quick_start {

class QuickStartMessage;

// Represents a high-level connection used to exchange Quick Start messages with
// the remote source device using a NearbyConnection.
class Connection
    : public TargetDeviceConnectionBroker::AuthenticatedConnection {
 public:
  static constexpr base::TimeDelta kDefaultRoundTripTimeout = base::Seconds(60);

  using HandshakeSuccessCallback = base::OnceCallback<void(bool)>;
  using ConnectionAuthenticatedCallback = base::OnceCallback<void(
      base::WeakPtr<TargetDeviceConnectionBroker::AuthenticatedConnection>)>;
  using ConnectionClosedCallback = base::OnceCallback<void(
      TargetDeviceConnectionBroker::ConnectionClosedReason)>;

  enum class State {
    kOpen,     // The NearbyConnection is Open
    kClosing,  // A close has been requested, but the connection is not yet
               // closed
    kClosed    // The connection is closed
  };

  class Factory {
   public:
    Factory() = default;
    Factory(const Factory&) = delete;
    Factory& operator=(const Factory&) = delete;
    virtual ~Factory();

    virtual std::unique_ptr<Connection> Create(
        NearbyConnection* nearby_connection,
        SessionContext* session_context,
        mojo::SharedRemote<mojom::QuickStartDecoder> quick_start_decoder,
        ConnectionClosedCallback on_connection_closed,
        ConnectionAuthenticatedCallback on_connection_authenticated);
  };

  Connection(NearbyConnection* nearby_connection,
             SessionContext* session_context,
             mojo::SharedRemote<mojom::QuickStartDecoder> quick_start_decoder,
             ConnectionClosedCallback on_connection_closed,
             ConnectionAuthenticatedCallback on_connection_authenticated);

  Connection(const Connection&) = delete;
  Connection& operator=(const Connection&) = delete;
  ~Connection() override;

  // Get the state of the connection (open, closing, or closed)
  State GetState();

  // TargetDeviceConnectionBroker::AuthenticatedConnection:
  void Close(
      TargetDeviceConnectionBroker::ConnectionClosedReason reason) override;

  // Changes the connection state to authenticated and invokes the
  // ConnectionAuthenticatedCallback. The caller must ensure that the connection
  // is authenticated before calling this function.
  void MarkConnectionAuthenticated(
      QuickStartMetrics::AuthenticationMethod auth_method);

  // Sends a cryptographic challenge to the source device. If the source device
  // can prove that it posesses the shared secret, then the connection is
  // authenticated. If the callback returns true, then the handshake has
  // succeeded; otherwise, the handshake has failed, which may mean that the
  // source device is untrustworthy and the target device should close the
  // connection. This is virtual to allow for overriding in unit tests.
  virtual void InitiateHandshake(const std::string& authentication_token,
                                 HandshakeSuccessCallback callback);

 private:
  friend class ConnectionTest;

  using ConnectionResponseCallback =
      base::OnceCallback<void(std::optional<std::vector<uint8_t>>)>;
  using PayloadResponseCallback =
      base::OnceCallback<void(std::optional<std::vector<uint8_t>>)>;
  using OnDecodingCompleteCallback =
      base::OnceCallback<void(mojom::QuickStartMessagePtr)>;

  // TargetDeviceConnectionBroker::AuthenticatedConnection:
  void RequestWifiCredentials(RequestWifiCredentialsCallback callback) override;
  void NotifySourceOfUpdate(NotifySourceOfUpdateCallback callback) override;
  void RequestAccountInfo(RequestAccountInfoCallback callback) override;
  void RequestAccountTransferAssertion(
      const Base64UrlString& challenge,
      RequestAccountTransferAssertionCallback callback) override;
  void WaitForUserVerification(AwaitUserVerificationCallback callback) override;
  base::Value::Dict GetPrepareForUpdateInfo() override;
  void NotifyPhoneSetupComplete() override;

  void DoWaitForUserVerification(size_t attempt_number,
                                 AwaitUserVerificationCallback callback);

  // Called each time any one of the three user verification packets
  // (UserVerificationRequested, UserVerificationMethod,
  // UserVerificationResponse) is decoded. |attempt_number| tracks how many user
  // verification packets have been decoded, since only three are expected.
  void OnUserVerificationPacketDecoded(
      size_t attempt_number,
      AwaitUserVerificationCallback callback,
      mojom::QuickStartMessagePtr quick_start_message);

  void OnNotifySourceOfUpdateResponse(
      NotifySourceOfUpdateCallback callback,
      mojom::QuickStartMessagePtr quick_start_message);

  // Parses a raw AssertionResponse and converts it into a FidoAssertionInfo
  void OnRequestAccountTransferAssertionResponse(
      RequestAccountTransferAssertionCallback callback,
      mojom::QuickStartMessagePtr quick_start_message);

  void OnBootstrapConfigurationsResponse(
      RequestAccountInfoCallback callback,
      mojom::QuickStartMessagePtr quick_start_message);

  void SendMessageAndDecodeResponse(
      std::unique_ptr<QuickStartMessage> message,
      QuickStartResponseType response_type,
      OnDecodingCompleteCallback callback,
      base::TimeDelta timeout = kDefaultRoundTripTimeout);
  void SendMessageAndDiscardResponse(
      std::unique_ptr<QuickStartMessage> message,
      QuickStartResponseType response_type,
      base::OnceClosure callback,
      base::TimeDelta timeout = kDefaultRoundTripTimeout);
  void SendMessageWithoutResponse(std::unique_ptr<QuickStartMessage> message,
                                  QuickStartResponseType message_type);
  void SendBytesAndReadResponse(
      std::vector<uint8_t>&& bytes,
      QuickStartResponseType response_type,
      ConnectionResponseCallback callback,
      base::TimeDelta timeout = kDefaultRoundTripTimeout);

  void OnHandshakeResponse(const std::string& authentication_token,
                           HandshakeSuccessCallback callback,
                           std::optional<std::vector<uint8_t>> response_bytes);

  void OnConnectionClosed(
      TargetDeviceConnectionBroker::ConnectionClosedReason reason);

  void OnResponseTimeout(QuickStartResponseType response_type);
  void OnResponseReceived(ConnectionResponseCallback callback,
                          QuickStartResponseType response_type,
                          std::optional<std::vector<uint8_t>> response_bytes);

  // Generic method to decode data using QuickStartDecoder. If a decoding error
  // occurs, return invoke |on_decoding_complete| with nullptr. On success,
  // |on_decoding_complete| will be called with the decoded data.
  void DecodeQuickStartMessage(OnDecodingCompleteCallback on_decoding_complete,
                               std::optional<std::vector<uint8_t>> data);

  base::OneShotTimer response_timeout_timer_;
  raw_ptr<NearbyConnection> nearby_connection_;
  raw_ptr<SessionContext> session_context_;
  State connection_state_ = State::kOpen;
  ConnectionClosedCallback on_connection_closed_;
  bool authenticated_ = false;
  ConnectionAuthenticatedCallback on_connection_authenticated_;
  std::string challenge_b64url_;
  mojo::SharedRemote<mojom::QuickStartDecoder> decoder_;
  std::unique_ptr<AccountTransferClientData> client_data_;
  std::unique_ptr<QuickStartMetrics> quick_start_metrics_;

  // Separate WeakPtrFactory for use with |OnResponseReceived()| to allow for
  // canceling the response.
  base::WeakPtrFactory<Connection> response_weak_ptr_factory_{this};

  base::WeakPtrFactory<Connection> weak_ptr_factory_{this};
};

}  // namespace ash::quick_start

#endif  // CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_CONNECTIVITY_CONNECTION_H_
