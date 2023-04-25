// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/oobe_quick_start/connectivity/connection.h"

#include <array>

#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/notreached.h"
#include "base/values.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/fido_assertion_info.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/random_session_id.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/target_device_connection_broker.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/wifi_credentials.h"
#include "chrome/browser/ash/login/oobe_quick_start/logging/logging.h"
#include "chrome/browser/nearby_sharing/public/cpp/nearby_connection.h"
#include "chromeos/ash/components/quick_start/quick_start_message.h"
#include "chromeos/ash/components/quick_start/quick_start_requests.h"
#include "chromeos/ash/services/nearby/public/mojom/quick_start_decoder.mojom.h"
#include "chromeos/ash/services/nearby/public/mojom/quick_start_decoder_types.mojom-shared.h"
#include "chromeos/ash/services/nearby/public/mojom/quick_start_decoder_types.mojom.h"
#include "crypto/random.h"

namespace ash::quick_start {

Connection::Factory::~Factory() = default;

std::unique_ptr<Connection> Connection::Factory::Create(
    NearbyConnection* nearby_connection,
    RandomSessionId session_id,
    SharedSecret shared_secret,
    SharedSecret secondary_shared_secret,
    ConnectionClosedCallback on_connection_closed,
    ConnectionAuthenticatedCallback on_connection_authenticated) {
  auto nonce_generator = std::make_unique<NonceGenerator>();
  return std::make_unique<Connection>(
      nearby_connection, session_id, shared_secret, secondary_shared_secret,
      std::move(nonce_generator), std::move(on_connection_closed),
      std::move(on_connection_authenticated));
}

Connection::Nonce Connection::NonceGenerator::Generate() {
  Nonce nonce;
  crypto::RandBytes(nonce);
  return nonce;
}

Connection::Connection(
    NearbyConnection* nearby_connection,
    RandomSessionId session_id,
    SharedSecret shared_secret,
    SharedSecret secondary_shared_secret,
    std::unique_ptr<NonceGenerator> nonce_generator,
    ConnectionClosedCallback on_connection_closed,
    ConnectionAuthenticatedCallback on_connection_authenticated)
    : nearby_connection_(nearby_connection),
      random_session_id_(session_id),
      shared_secret_(shared_secret),
      secondary_shared_secret_(secondary_shared_secret),
      nonce_generator_(std::move(nonce_generator)),
      on_connection_closed_(std::move(on_connection_closed)),
      on_connection_authenticated_(std::move(on_connection_authenticated)) {
  // Since we aren't expecting any disconnections, treat any drops of the
  // connection as an unknown error.
  nearby_connection->SetDisconnectionListener(base::BindOnce(
      &Connection::OnConnectionClosed, weak_ptr_factory_.GetWeakPtr(),
      TargetDeviceConnectionBroker::ConnectionClosedReason::kUnknownError));
}

Connection::~Connection() = default;

void Connection::MarkConnectionAuthenticated() {
  authenticated_ = true;
  if (on_connection_authenticated_) {
    std::move(on_connection_authenticated_).Run(weak_ptr_factory_.GetWeakPtr());
  }
}

Connection::State Connection::GetState() {
  return connection_state_;
}

void Connection::Close(
    TargetDeviceConnectionBroker::ConnectionClosedReason reason) {
  CHECK(connection_state_ == State::kOpen);

  connection_state_ = State::kClosing;

  // Update the disconnect listener to treat disconnections as the reason listed
  // above.
  nearby_connection_->SetDisconnectionListener(base::BindOnce(
      &Connection::OnConnectionClosed, weak_ptr_factory_.GetWeakPtr(), reason));

  // Issue a close
  nearby_connection_->Close();
}

void Connection::RequestWifiCredentials(
    int32_t session_id,
    RequestWifiCredentialsCallback callback) {
  // Build the Wifi Credential Request payload
  std::string shared_secret_str(secondary_shared_secret_.begin(),
                                secondary_shared_secret_.end());
  SendMessage(
      requests::BuildRequestWifiCredentialsMessage(session_id,
                                                   shared_secret_str),
      base::BindOnce(&Connection::OnRequestWifiCredentialsResponse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void Connection::NotifySourceOfUpdate(int32_t session_id) {
  // Send message to source that target device will perform an update.
  std::string shared_secret_str(secondary_shared_secret_.begin(),
                                secondary_shared_secret_.end());

  SendMessage(
      requests::BuildNotifySourceOfUpdateMessage(session_id, shared_secret_str),
      absl::nullopt);
}

void Connection::RequestAccountTransferAssertion(
    const std::string& challenge_b64url,
    RequestAccountTransferAssertionCallback callback) {
  auto parse_assertion_response =
      base::BindOnce(&Connection::OnRequestAccountTransferAssertionResponse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback));

  auto request_assertion =
      base::IgnoreArgs<absl::optional<std::vector<uint8_t>>>(base::BindOnce(
          &Connection::SendMessage, weak_ptr_factory_.GetWeakPtr(),
          requests::BuildAssertionRequestMessage(challenge_b64url),
          std::move(parse_assertion_response)));

  // Set up a callback to call GetInfo, calling back into RequestAssertion
  // (and ignoring the results of GetInfo) after the call succeeds.
  auto get_info = base::IgnoreArgs<absl::optional<std::vector<uint8_t>>>(
      base::BindOnce(&Connection::SendMessage, weak_ptr_factory_.GetWeakPtr(),
                     requests::BuildGetInfoRequestMessage(),
                     std::move(request_assertion)));

  // Call into SetBootstrapOptions, starting the chain of callbacks.
  SendMessage(requests::BuildBootstrapOptionsRequest(), std::move(get_info));
}

void Connection::OnRequestWifiCredentialsResponse(
    RequestWifiCredentialsCallback callback,
    absl::optional<std::vector<uint8_t>> response_bytes) {
  if (!response_bytes.has_value()) {
    QS_LOG(ERROR) << "No response bytes received for wifi credentials request";
    std::move(callback).Run(absl::nullopt);
    return;
  }

  auto parse_mojo_response_callback =
      base::BindOnce(&Connection::ParseWifiCredentialsResponse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback));

  decoder_->DecodeWifiCredentialsResponse(
      response_bytes.value(), std::move(parse_mojo_response_callback));
}

void Connection::ParseWifiCredentialsResponse(
    RequestWifiCredentialsCallback callback,
    ::ash::quick_start::mojom::GetWifiCredentialsResponsePtr response) {
  if (!response->is_credentials()) {
    std::move(callback).Run(absl::nullopt);
    return;
  }

  WifiCredentials credentials;
  credentials.ssid = response->get_credentials()->ssid;
  credentials.password = response->get_credentials()->password;
  credentials.is_hidden = response->get_credentials()->is_hidden;
  credentials.security_type = response->get_credentials()->security_type;
  std::move(callback).Run(std::move(credentials));
}

void Connection::OnRequestAccountTransferAssertionResponse(
    RequestAccountTransferAssertionCallback callback,
    absl::optional<std::vector<uint8_t>> response_bytes) {
  if (!response_bytes.has_value()) {
    std::move(callback).Run(absl::nullopt);
    return;
  }

  auto parse_mojo_response_callback =
      base::BindOnce(&Connection::GenerateFidoAssertionInfo,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback));

  decoder_->DecodeGetAssertionResponse(response_bytes.value(),
                                       std::move(parse_mojo_response_callback));
}

void Connection::GenerateFidoAssertionInfo(
    RequestAccountTransferAssertionCallback callback,
    ::ash::quick_start::mojom::GetAssertionResponsePtr response) {
  mojom::GetAssertionResponse::GetAssertionStatus success =
      mojom::GetAssertionResponse::GetAssertionStatus::kSuccess;

  if (response->status != success) {
    std::move(callback).Run(absl::nullopt);
    return;
  }

  FidoAssertionInfo assertion_info;
  assertion_info.email = response->email;
  assertion_info.credential_id = response->credential_id;
  assertion_info.authenticator_data = response->auth_data;
  assertion_info.signature = response->signature;

  std::move(callback).Run(assertion_info);
}

void Connection::SendMessage(
    std::unique_ptr<QuickStartMessage> message,
    absl::optional<ConnectionResponseCallback> callback) {
  SendPayload(*message->GenerateEncodedMessage());
  if (!callback.has_value()) {
    return;
  }

  nearby_connection_->Read(std::move(callback.value()));
}

void Connection::InitiateHandshake(const std::string& authentication_token,
                                   HandshakeSuccessCallback callback) {
  Connection::Nonce nonce = nonce_generator_->Generate();
  nearby_connection_->Write(requests::BuildTargetDeviceHandshakeMessage(
      authentication_token, shared_secret_, nonce));

  // TODO(b/234655072): Read response from phone and run callback.
}

void Connection::SendPayload(const base::Value::Dict& message_payload) {
  std::string json_serialized_payload;
  CHECK(base::JSONWriter::Write(message_payload, &json_serialized_payload));
  std::vector<uint8_t> request_payload(json_serialized_payload.begin(),
                                       json_serialized_payload.end());
  nearby_connection_->Write(request_payload);
}

void Connection::SendPayloadAndReadResponse(
    const base::Value::Dict& message_payload,
    PayloadResponseCallback callback) {
  SendPayload(message_payload);
  nearby_connection_->Read(std::move(callback));
}

void Connection::OnConnectionClosed(
    TargetDeviceConnectionBroker::ConnectionClosedReason reason) {
  connection_state_ = Connection::State::kClosed;
  std::move(on_connection_closed_).Run(reason);
}

}  // namespace ash::quick_start
