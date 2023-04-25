// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_CONNECTIVITY_CONNECTION_H_
#define CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_CONNECTIVITY_CONNECTION_H_

#include <memory>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/fido_assertion_info.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/random_session_id.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/target_device_connection_broker.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/wifi_credentials.h"
#include "chrome/browser/nearby_sharing/public/cpp/nearby_connection.h"
#include "chromeos/ash/services/nearby/public/mojom/quick_start_decoder.mojom.h"
#include "components/cbor/values.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "url/origin.h"

namespace ash::quick_start {

class QuickStartMessage;

// Represents a connection to the remote source device and is an abstraction of
// a Nearby Connection.
class Connection
    : public TargetDeviceConnectionBroker::AuthenticatedConnection {
 public:
  using SharedSecret = TargetDeviceConnectionBroker::SharedSecret;
  using Nonce = std::array<uint8_t, 12>;
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
        RandomSessionId session_id,
        SharedSecret shared_secret,
        SharedSecret secondary_shared_secret,
        ConnectionClosedCallback on_connection_closed,
        ConnectionAuthenticatedCallback on_connection_authenticated);
  };

  class NonceGenerator {
   public:
    NonceGenerator() = default;
    NonceGenerator(const NonceGenerator&) = delete;
    NonceGenerator& operator=(const NonceGenerator&) = delete;
    virtual ~NonceGenerator() = default;

    virtual Nonce Generate();
  };

  Connection(NearbyConnection* nearby_connection,
             RandomSessionId session_id,
             SharedSecret shared_secret,
             SharedSecret secondary_shared_secret,
             std::unique_ptr<NonceGenerator> nonce_generator,
             ConnectionClosedCallback on_connection_closed,
             ConnectionAuthenticatedCallback on_connection_authenticated);

  Connection(const Connection&) = delete;
  Connection& operator=(const Connection&) = delete;
  ~Connection() override;

  // Get the state of the connection (open, closing, or closed)
  State GetState();

  // Close the connection.
  void Close(TargetDeviceConnectionBroker::ConnectionClosedReason reason);

  // Changes the connection state to authenticated and invokes the
  // ConnectionAuthenticatedCallback. The caller must ensure that the connection
  // is authenticated before calling this function.
  void MarkConnectionAuthenticated();

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
      base::OnceCallback<void(absl::optional<std::vector<uint8_t>>)>;
  using PayloadResponseCallback =
      base::OnceCallback<void(absl::optional<std::vector<uint8_t>>)>;

  // TargetDeviceConnectionBroker::AuthenticatedConnection
  void RequestWifiCredentials(int32_t session_id,
                              RequestWifiCredentialsCallback callback) override;
  void NotifySourceOfUpdate(int32_t session_id) override;
  void RequestAccountTransferAssertion(
      const std::string& challenge_b64url,
      RequestAccountTransferAssertionCallback callback) override;

  // Parses a raw response and converts it to a WifiCredentialsResponse
  void OnRequestWifiCredentialsResponse(
      RequestWifiCredentialsCallback callback,
      absl::optional<std::vector<uint8_t>> response_bytes);

  void ParseWifiCredentialsResponse(
      RequestWifiCredentialsCallback callback,
      ::ash::quick_start::mojom::GetWifiCredentialsResponsePtr response);

  // Parses a raw AssertionResponse and converts it into a FidoAssertionInfo
  void OnRequestAccountTransferAssertionResponse(
      RequestAccountTransferAssertionCallback callback,
      absl::optional<std::vector<uint8_t>> response_bytes);

  void GenerateFidoAssertionInfo(
      RequestAccountTransferAssertionCallback callback,
      ::ash::quick_start::mojom::GetAssertionResponsePtr response);

  void SendMessage(std::unique_ptr<QuickStartMessage> message,
                   absl::optional<ConnectionResponseCallback> callback);

  // Reusable method to serialize a payload into JSON bytes and send via Nearby
  // Connections.
  void SendPayload(const base::Value::Dict& message_payload);

  void SendPayloadAndReadResponse(const base::Value::Dict& message_payload,
                                  PayloadResponseCallback callback);

  void OnConnectionClosed(
      TargetDeviceConnectionBroker::ConnectionClosedReason reason);

  raw_ptr<NearbyConnection, ExperimentalAsh> nearby_connection_;
  RandomSessionId random_session_id_;
  SharedSecret shared_secret_;
  SharedSecret secondary_shared_secret_;
  State connection_state_ = State::kOpen;
  std::unique_ptr<NonceGenerator> nonce_generator_;
  ConnectionClosedCallback on_connection_closed_;
  bool authenticated_ = false;
  ConnectionAuthenticatedCallback on_connection_authenticated_;
  std::string challenge_b64url_;
  mojo::SharedRemote<mojom::QuickStartDecoder> decoder_;

  base::WeakPtrFactory<Connection> weak_ptr_factory_{this};
};

}  // namespace ash::quick_start

#endif  // CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_CONNECTIVITY_CONNECTION_H_
