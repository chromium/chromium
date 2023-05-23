// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/oobe_quick_start/connectivity/connection.h"

#include <array>

#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/fido_assertion_info.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/random_session_id.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/target_device_connection_broker.h"
#include "chrome/browser/ash/login/oobe_quick_start/logging/logging.h"
#include "chrome/browser/nearby_sharing/public/cpp/nearby_connection.h"
#include "chromeos/ash/components/quick_start/quick_start_message.h"
#include "chromeos/ash/components/quick_start/quick_start_requests.h"
#include "chromeos/ash/services/nearby/public/mojom/quick_start_decoder.mojom.h"
#include "chromeos/ash/services/nearby/public/mojom/quick_start_decoder_types.mojom-forward.h"
#include "chromeos/ash/services/nearby/public/mojom/quick_start_decoder_types.mojom-shared.h"
#include "chromeos/ash/services/nearby/public/mojom/quick_start_decoder_types.mojom.h"
#include "crypto/random.h"

namespace ash::quick_start {

namespace {

constexpr base::TimeDelta kNotifySourceOfUpdateResponseTimeout =
    base::Seconds(3);

}  // namespace

Connection::Factory::~Factory() = default;

std::unique_ptr<Connection> Connection::Factory::Create(
    NearbyConnection* nearby_connection,
    Connection::SessionContext session_context,
    mojo::SharedRemote<mojom::QuickStartDecoder> quick_start_decoder,
    ConnectionClosedCallback on_connection_closed,
    ConnectionAuthenticatedCallback on_connection_authenticated) {
  auto nonce_generator = std::make_unique<NonceGenerator>();
  return std::make_unique<Connection>(
      nearby_connection, session_context, std::move(quick_start_decoder),
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
    Connection::SessionContext session_context,
    mojo::SharedRemote<mojom::QuickStartDecoder> quick_start_decoder,
    std::unique_ptr<NonceGenerator> nonce_generator,
    ConnectionClosedCallback on_connection_closed,
    ConnectionAuthenticatedCallback on_connection_authenticated)
    : nearby_connection_(nearby_connection),
      random_session_id_(session_context.session_id),
      shared_secret_(session_context.shared_secret),
      secondary_shared_secret_(session_context.secondary_shared_secret),
      nonce_generator_(std::move(nonce_generator)),
      on_connection_closed_(std::move(on_connection_closed)),
      on_connection_authenticated_(std::move(on_connection_authenticated)),
      decoder_(std::move(quick_start_decoder)) {
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
  if (response_timeout_timer_.IsRunning()) {
    response_timeout_timer_.Stop();
  }
  if (connection_state_ != State::kOpen) {
    return;
  }

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
  ConnectionResponseCallback on_response_received =
      base::BindOnce(&Connection::DecodeData<mojom::WifiCredentials>,
                     weak_ptr_factory_.GetWeakPtr(),
                     &mojom::QuickStartDecoder::DecodeWifiCredentialsResponse,
                     std::move(callback));
  SendMessage(requests::BuildRequestWifiCredentialsMessage(session_id,
                                                           shared_secret_str),
              std::move(on_response_received));
}

void Connection::NotifySourceOfUpdate(int32_t session_id,
                                      NotifySourceOfUpdateCallback callback) {
  // Send message to source that target device will perform an update.
  // TODO(b/234655072): Cleanup BuildNotifySourceOfUpdateMessage plumbing to
  // pass in session_id as a base::span<uint8_t,32> and avoid copying twice.
  std::string shared_secret_str(secondary_shared_secret_.begin(),
                                secondary_shared_secret_.end());

  response_timeout_timer_.Start(FROM_HERE, kNotifySourceOfUpdateResponseTimeout,
                                base::BindOnce(&Connection::OnResponseTimeout,
                                               weak_ptr_factory_.GetWeakPtr()));
  SendMessage(
      requests::BuildNotifySourceOfUpdateMessage(session_id, shared_secret_str),
      base::BindOnce(&Connection::OnNotifySourceOfUpdateResponse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
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

  auto bootstrap_configurations_response =
      base::BindOnce(&Connection::OnBootstrapConfigurationsResponse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(get_info));

  // Call into SetBootstrapOptions, starting the chain of callbacks.
  SendMessage(requests::BuildBootstrapOptionsRequest(),
              std::move(bootstrap_configurations_response));
}

void Connection::OnNotifySourceOfUpdateResponse(
    NotifySourceOfUpdateCallback callback,
    absl::optional<std::vector<uint8_t>> response_bytes) {
  response_timeout_timer_.Stop();

  if (!response_bytes.has_value()) {
    QS_LOG(ERROR)
        << "No response bytes received for notify source of update message";
    std::move(callback).Run(/*ack_received=*/false);
    return;
  }

  auto handle_mojo_response_callback =
      base::BindOnce(&Connection::HandleNotifySourceOfUpdateResponse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback));

  decoder_->DecodeNotifySourceOfUpdateResponse(
      response_bytes.value(), std::move(handle_mojo_response_callback));
}

void Connection::HandleNotifySourceOfUpdateResponse(
    NotifySourceOfUpdateCallback callback,
    absl::optional<bool> ack_received) {
  if (!ack_received.has_value()) {
    QS_LOG(ERROR)
        << "No ack received value in the NotifySourceOfUpdate response.";
    std::move(callback).Run(/*ack_successful=*/false);
    return;
  }

  if (!ack_received.value()) {
    QS_LOG(ERROR) << "The ack received value in the NotifySourceOfUpdate "
                     "response is unexpectedly 'false'.";
    std::move(callback).Run(/*ack_successful=*/false);
    return;
  }

  std::move(callback).Run(/*ack_successful=*/true);
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

void Connection::OnBootstrapConfigurationsResponse(
    BootstrapConfigurationsCallback callback,
    absl::optional<std::vector<uint8_t>> response_bytes) {
  if (!response_bytes.has_value()) {
    return;
  }

  // TODO(b/280306851): Finish parsing response and save cryptauth_device_id.
  DecodeData<mojom::BootstrapConfigurations>(
      &mojom::QuickStartDecoder::DecodeBootstrapConfigurations,
      base::DoNothing(), std::move(response_bytes));

  std::move(callback).Run(absl::nullopt);
}

void Connection::SendMessage(std::unique_ptr<QuickStartMessage> message,
                             ConnectionResponseCallback callback) {
  SendPayload(*message->GenerateEncodedMessage());
  nearby_connection_->Read(std::move(callback));
}

void Connection::InitiateHandshake(const std::string& authentication_token,
                                   HandshakeSuccessCallback callback) {
  Connection::Nonce nonce = nonce_generator_->Generate();
  nearby_connection_->Write(requests::BuildTargetDeviceHandshakeMessage(
      authentication_token, shared_secret_, nonce));

  // TODO(b/234655072): Read response from phone and run callback.
}

void Connection::WaitForUserVerification(
    AwaitUserVerificationCallback callback) {
  auto on_decoding_completed =
      base::BindOnce(&Connection::OnUserVerificationRequested,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback));

  ConnectionResponseCallback on_message_received =
      base::BindOnce(&Connection::DecodeData<mojom::UserVerificationRequested>,
                     weak_ptr_factory_.GetWeakPtr(),
                     &mojom::QuickStartDecoder::DecodeUserVerificationRequested,
                     std::move(on_decoding_completed));

  nearby_connection_->Read(std::move(on_message_received));
}

void Connection::OnUserVerificationRequested(
    AwaitUserVerificationCallback callback,
    absl::optional<mojom::UserVerificationRequested>
        user_verification_request) {
  if (!user_verification_request.has_value()) {
    QS_LOG(ERROR) << "No user verification request received from phone.";
    std::move(callback).Run(absl::nullopt);
    return;
  }

  if (!user_verification_request->is_awaiting_user_verification) {
    QS_LOG(ERROR) << "User verification request received from phone, but "
                     "is_awaiting_user_verification is false.";

    std::move(callback).Run(absl::nullopt);
    return;
  }

  ConnectionResponseCallback on_response_received =
      base::BindOnce(&Connection::DecodeData<mojom::UserVerificationResponse>,
                     weak_ptr_factory_.GetWeakPtr(),
                     &mojom::QuickStartDecoder::DecodeUserVerificationResult,
                     std::move(callback));

  nearby_connection_->Read(std::move(on_response_received));
}

template <typename T>
void Connection::DecodeData(DecoderMethod<T> decoder_method,
                            OnDecodingCompleteCallback<T> on_decoding_complete,
                            absl::optional<std::vector<uint8_t>> data) {
  if (!data.has_value()) {
    QS_LOG(ERROR) << "No data received from phone.";
    std::move(on_decoding_complete).Run(absl::nullopt);
    return;
  }

  // Setup a callback to handle the decoder's response. If an error was
  // reported, return empty. If not, run the success callback with the
  // decoded data.
  DecoderResponseCallback<T> decoder_callback = base::BindOnce(
      [](OnDecodingCompleteCallback<T> on_decoding_complete,
         mojo::InlinedStructPtr<T> data,
         absl::optional<mojom::QuickStartDecoderError> error) {
        if (error.has_value()) {
          // TODO(b/281052191): Log error code here
          QS_LOG(ERROR) << "Error decoding data.";
          std::move(on_decoding_complete).Run(absl::nullopt);
          return;
        }

        std::move(on_decoding_complete).Run(*std::move(data));
      },
      std::move(on_decoding_complete));

  // Run the decoder
  base::BindOnce(decoder_method, decoder_, data.value(),
                 std::move(decoder_callback))
      .Run();
}

void Connection::SendPayload(const base::Value::Dict& message_payload) {
  std::string json_serialized_payload;
  CHECK(base::JSONWriter::Write(message_payload, &json_serialized_payload));
  std::vector<uint8_t> request_payload(json_serialized_payload.begin(),
                                       json_serialized_payload.end());
  nearby_connection_->Write(request_payload);
}

void Connection::OnConnectionClosed(
    TargetDeviceConnectionBroker::ConnectionClosedReason reason) {
  connection_state_ = Connection::State::kClosed;
  std::move(on_connection_closed_).Run(reason);
}

void Connection::OnResponseTimeout() {
  QS_LOG(ERROR) << "Timed out waiting for a response from source device.";
  Close(TargetDeviceConnectionBroker::ConnectionClosedReason::kResponseTimeout);
}

}  // namespace ash::quick_start
