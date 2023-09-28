// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/oobe_quick_start/connectivity/connection.h"

#include <array>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/fido_assertion_info.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/handshake_helpers.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/session_context.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/target_device_connection_broker.h"
#include "chrome/browser/nearby_sharing/public/cpp/nearby_connection.h"
#include "chromeos/ash/components/quick_start/logging.h"
#include "chromeos/ash/components/quick_start/quick_start_message.h"
#include "chromeos/ash/components/quick_start/quick_start_metrics.h"
#include "chromeos/ash/components/quick_start/quick_start_requests.h"
#include "chromeos/ash/services/nearby/public/mojom/quick_start_decoder.mojom.h"
#include "chromeos/ash/services/nearby/public/mojom/quick_start_decoder_types.mojom-forward.h"
#include "chromeos/ash/services/nearby/public/mojom/quick_start_decoder_types.mojom-shared.h"
#include "chromeos/ash/services/nearby/public/mojom/quick_start_decoder_types.mojom.h"

namespace ash::quick_start {

namespace {

// TODO(b/280308144): Delete this switch once the host device handles the
// NotifySourceOfUpdate message. This is used to manually test forced update
// before Android implements the NotifySourceOfUpdate ack response.
constexpr char kQuickStartTestForcedUpdateSwitch[] =
    "quick-start-test-forced-update";

}  // namespace

Connection::Factory::~Factory() = default;

std::unique_ptr<Connection> Connection::Factory::Create(
    NearbyConnection* nearby_connection,
    SessionContext session_context,
    mojo::SharedRemote<mojom::QuickStartDecoder> quick_start_decoder,
    ConnectionClosedCallback on_connection_closed,
    ConnectionAuthenticatedCallback on_connection_authenticated) {
  return std::make_unique<Connection>(
      nearby_connection, session_context, std::move(quick_start_decoder),
      std::move(on_connection_closed), std::move(on_connection_authenticated));
}

Connection::Connection(
    NearbyConnection* nearby_connection,
    SessionContext session_context,
    mojo::SharedRemote<mojom::QuickStartDecoder> quick_start_decoder,
    ConnectionClosedCallback on_connection_closed,
    ConnectionAuthenticatedCallback on_connection_authenticated)
    : nearby_connection_(nearby_connection),
      session_context_(session_context),
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
    RequestWifiCredentialsCallback callback) {
  // Build the Wifi Credential Request payload
  SessionContext::SharedSecret secondary_shared_secret =
      session_context_.secondary_shared_secret();
  std::string shared_secret_str(secondary_shared_secret.begin(),
                                secondary_shared_secret.end());
  ConnectionResponseCallback on_response_received =
      base::BindOnce(&Connection::DecodeData<mojom::WifiCredentials>,
                     weak_ptr_factory_.GetWeakPtr(),
                     &mojom::QuickStartDecoder::DecodeWifiCredentialsResponse,
                     std::move(callback));
  SendMessageAndReadResponse(
      requests::BuildRequestWifiCredentialsMessage(
          session_context_.session_id(), shared_secret_str),
      QuickStartResponseType::kWifiCredentials,
      std::move(on_response_received));
}

void Connection::NotifySourceOfUpdate(NotifySourceOfUpdateCallback callback) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          kQuickStartTestForcedUpdateSwitch)) {
    HandleNotifySourceOfUpdateResponse(
        std::move(callback),
        mojom::NotifySourceOfUpdateResponse::New(/*ack_received=*/true),
        /*error=*/absl::nullopt);
    return;
  }

  SessionContext::SharedSecret secondary_shared_secret =
      session_context_.secondary_shared_secret();
  SendMessageAndReadResponse(
      requests::BuildNotifySourceOfUpdateMessage(session_context_.session_id(),
                                                 secondary_shared_secret),
      QuickStartResponseType::kNotifySourceOfUpdate,
      base::BindOnce(&Connection::OnNotifySourceOfUpdateResponse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void Connection::RequestAccountTransferAssertion(
    const Base64UrlString& challenge,
    RequestAccountTransferAssertionCallback callback) {
  client_data_ = std::make_unique<AccountTransferClientData>(challenge);

  auto parse_assertion_response =
      base::BindOnce(&Connection::OnRequestAccountTransferAssertionResponse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback));

  auto request_assertion =
      base::IgnoreArgs<absl::optional<std::vector<uint8_t>>>(base::BindOnce(
          &Connection::SendMessageAndReadResponse,
          weak_ptr_factory_.GetWeakPtr(),
          requests::BuildAssertionRequestMessage(client_data_->CreateHash()),
          QuickStartResponseType::kAssertion,
          std::move(parse_assertion_response), kDefaultRoundTripTimeout));

  // Set up a callback to call GetInfo, calling back into RequestAssertion
  // (and ignoring the results of GetInfo) after the call succeeds.
  auto get_info = base::IgnoreArgs<absl::optional<std::vector<uint8_t>>>(
      base::BindOnce(&Connection::SendMessageAndReadResponse,
                     weak_ptr_factory_.GetWeakPtr(),
                     requests::BuildGetInfoRequestMessage(),
                     QuickStartResponseType::kGetInfo,
                     std::move(request_assertion), kDefaultRoundTripTimeout));

  auto bootstrap_configurations_response =
      base::BindOnce(&Connection::OnBootstrapConfigurationsResponse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(get_info));

  // Call into SetBootstrapOptions, starting the chain of callbacks.
  SendMessageAndReadResponse(requests::BuildBootstrapOptionsRequest(),
                             QuickStartResponseType::kBootstrapConfigurations,
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
    mojom::NotifySourceOfUpdateResponsePtr notify_source_of_update_response,
    absl::optional<mojom::QuickStartDecoderError> error) {
  if (!notify_source_of_update_response) {
    CHECK(error.has_value());
    QS_LOG(ERROR)
        << "No ack received value in the NotifySourceOfUpdate response.";
    std::move(callback).Run(/*ack_successful=*/false);
    return;
  }

  if (!notify_source_of_update_response->ack_received) {
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
    quick_start_metrics::RecordGaiaTransferResult(
        /*succeeded=*/false, /*failure_reason=*/quick_start_metrics::
            GaiaTransferResultFailureReason::kNoAccountsReceivedFromPhone);
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
    ash::quick_start::mojom::FidoAssertionResponsePtr fido_response,
    absl::optional<::ash::quick_start::mojom::QuickStartDecoderError> error) {
  // TODO (b/279614284): Emit metric for Gaia transfer failure reasons when
  // unknown message logic is finalized.
  if (error.has_value()) {
    // TODO (b/286877412): Update this logic once we've aligned on an unknown
    // message strategy.
    QS_LOG(INFO) << "Ignoring message and re-reading";
    nearby_connection_->Read(
        base::BindOnce(&Connection::OnRequestAccountTransferAssertionResponse,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
    return;
  }

  FidoAssertionInfo assertion_info;
  assertion_info.email = fido_response->email;
  assertion_info.credential_id = fido_response->credential_id;
  assertion_info.authenticator_data = fido_response->auth_data;
  assertion_info.signature = fido_response->signature;

  quick_start_metrics::RecordGaiaTransferResult(
      /*succeeded=*/true, /*failure_reason=*/absl::nullopt);

  std::move(callback).Run(assertion_info);
}

void Connection::OnBootstrapConfigurationsResponse(
    BootstrapConfigurationsCallback callback,
    absl::optional<std::vector<uint8_t>> response_bytes) {
  if (!response_bytes.has_value()) {
    return;
  }

  auto on_decoding_completed =
      base::BindOnce(&Connection::ParseBootstrapConfigurationsResponse,
                     weak_ptr_factory_.GetWeakPtr());

  DecodeData<mojom::BootstrapConfigurations>(
      &mojom::QuickStartDecoder::DecodeBootstrapConfigurations,
      std::move(on_decoding_completed), std::move(response_bytes));

  std::move(callback).Run(absl::nullopt);
}

void Connection::ParseBootstrapConfigurationsResponse(
    absl::optional<mojom::BootstrapConfigurations> bootstrap_configurations) {
  phone_instance_id_ = bootstrap_configurations->cryptauth_device_id;
}

void Connection::SendMessageAndReadResponse(
    std::unique_ptr<QuickStartMessage> message,
    QuickStartResponseType response_type,
    ConnectionResponseCallback callback,
    base::TimeDelta timeout) {
  std::string json_serialized_payload;
  CHECK(base::JSONWriter::Write(*message->GenerateEncodedMessage(),
                                &json_serialized_payload));

  SendBytesAndReadResponse(std::vector<uint8_t>(json_serialized_payload.begin(),
                                                json_serialized_payload.end()),
                           response_type, std::move(callback), timeout);
}

void Connection::SendBytesAndReadResponse(std::vector<uint8_t>&& bytes,
                                          QuickStartResponseType response_type,
                                          ConnectionResponseCallback callback,
                                          base::TimeDelta timeout) {
  quick_start_metrics::RecordMessageSent(
      quick_start_metrics::MapResponseToMessageType(response_type));
  nearby_connection_->Write(std::move(bytes));
  nearby_connection_->Read(base::BindOnce(
      &Connection::OnResponseReceived, response_weak_ptr_factory_.GetWeakPtr(),
      std::move(callback), response_type));

  message_elapsed_timer_ = std::make_unique<base::ElapsedTimer>();
  response_timeout_timer_.Start(
      FROM_HERE, timeout,
      base::BindOnce(&Connection::OnResponseTimeout,
                     weak_ptr_factory_.GetWeakPtr(), response_type));
}

void Connection::InitiateHandshake(const std::string& authentication_token,
                                   HandshakeSuccessCallback callback) {
  SendBytesAndReadResponse(
      handshake::BuildHandshakeMessage(authentication_token,
                                       session_context_.shared_secret()),
      QuickStartResponseType::kHandshake,
      base::BindOnce(&Connection::OnHandshakeResponse,
                     weak_ptr_factory_.GetWeakPtr(), authentication_token,
                     std::move(callback)));
  handshake_elapsed_timer_ = std::make_unique<base::ElapsedTimer>();
}

void Connection::OnHandshakeResponse(
    const std::string& authentication_token,
    HandshakeSuccessCallback callback,
    absl::optional<std::vector<uint8_t>> response_bytes) {
  if (!response_bytes) {
    QS_LOG(ERROR) << "Failed to read handshake response from NearbyConnection";
    quick_start_metrics::RecordHandshakeResult(
        /*success=*/false, /*duration=*/handshake_elapsed_timer_->Elapsed(),
        /*error_code=*/
        quick_start_metrics::HandshakeErrorCode::kFailedToReadResponse);
    handshake_elapsed_timer_.reset();
    std::move(callback).Run(/*success=*/false);
    return;
  }
  handshake::VerifyHandshakeMessageStatus status =
      handshake::VerifyHandshakeMessage(*response_bytes, authentication_token,
                                        session_context_.shared_secret());
  bool success = status == handshake::VerifyHandshakeMessageStatus::kSuccess;
  if (success) {
    quick_start_metrics::RecordHandshakeResult(
        /*success=*/true, /*duration=*/handshake_elapsed_timer_->Elapsed(),
        /*error_code=*/absl::nullopt);
  } else {
    quick_start_metrics::RecordHandshakeResult(
        /*success=*/false, /*duration=*/handshake_elapsed_timer_->Elapsed(),
        /*error_code=*/
        handshake::MapHandshakeStatusToErrorCode(status));
  }
  handshake_elapsed_timer_.reset();
  std::move(callback).Run(success);
}

void Connection::WaitForUserVerification(
    AwaitUserVerificationCallback callback) {
  auto [on_user_verification_complete, on_error] =
      base::SplitOnceCallback(std::move(callback));
  auto on_user_verification_requested_decoded = base::BindOnce(
      &Connection::OnUserVerificationRequested, weak_ptr_factory_.GetWeakPtr(),
      std::move(on_user_verification_complete));

  ConnectionResponseCallback decode_user_verification_requested =
      base::BindOnce(&Connection::DecodeData<mojom::UserVerificationRequested>,
                     weak_ptr_factory_.GetWeakPtr(),
                     &mojom::QuickStartDecoder::DecodeUserVerificationRequested,
                     std::move(on_user_verification_requested_decoded));

  // We decode the user verification packet either (1) when the first packet
  // received fails to parse as a user verification method, or (2) when we
  // finish handling the user verification method and we receive the second
  // packet.
  auto [on_decode_user_verification_method_failure, on_receive_second_packet] =
      base::SplitOnceCallback(std::move(decode_user_verification_requested));

  OnDecodingSuccessCallback<mojom::UserVerificationMethod>
      on_decode_user_verification_method_success = base::BindOnce(
          [](AwaitUserVerificationCallback callback,
             base::OnceClosure read_next_packet,
             mojom::UserVerificationMethod user_verification_method) {
            if (!user_verification_method.use_source_lock_screen_prompt) {
              QS_LOG(ERROR) << "Unsupported user verification method";
              std::move(callback).Run(absl::nullopt);
              return;
            }
            std::move(read_next_packet).Run();
          },
          std::move(on_error),
          base::BindOnce(&NearbyConnection::Read,
                         nearby_connection_->GetWeakPtr(),
                         std::move(on_receive_second_packet)));

  ConnectionResponseCallback try_decode_user_verification_method =
      base::BindOnce(&Connection::TryDecodeData<mojom::UserVerificationMethod>,
                     weak_ptr_factory_.GetWeakPtr(),
                     &mojom::QuickStartDecoder::DecodeUserVerificationMethod,
                     std::move(on_decode_user_verification_method_success),
                     std::move(on_decode_user_verification_method_failure));

  nearby_connection_->Read(std::move(try_decode_user_verification_method));
}

base::Value::Dict Connection::GetPrepareForUpdateInfo() {
  return session_context_.GetPrepareForUpdateInfo();
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
  // Setup a callback to handle the decoder's response. If an error was
  // reported, return empty. If not, run the success callback with the
  // decoded data.
  // TODO(b/302034651): Refactor to avoid using InlinedStructPtr like this.
  // Whether an InlinedStructPtr or a StructPtr is to be used is an
  // implementation detail, and code like this is fragile.
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
  base::BindOnce(decoder_method, decoder_, data, std::move(decoder_callback))
      .Run();
}

template <typename T>
void Connection::TryDecodeData(DecoderMethod<T> decoder_method,
                               OnDecodingSuccessCallback<T> on_decoding_success,
                               ConnectionResponseCallback on_decoding_failed,
                               absl::optional<std::vector<uint8_t>> data) {
  // Setup a callback to handle the decoder's response. If an error was
  // reported, run the failure callback with the original data. If not, run the
  // success callback with the decoded data.
  // TODO(b/302034651): Refactor to avoid using InlinedStructPtr like this.
  // Whether an InlinedStructPtr or a StructPtr is to be used is an
  // implementation detail, and code like this is fragile.
  DecoderResponseCallback<T> decoder_callback = base::BindOnce(
      [](OnDecodingSuccessCallback<T> on_decoding_success,
         ConnectionResponseCallback on_decoding_failed,
         absl::optional<std::vector<uint8_t>> data,
         mojo::InlinedStructPtr<T> result,
         absl::optional<mojom::QuickStartDecoderError> error) {
        if (error.has_value() || !result) {
          // TODO(b/281052191): Log error code here
          QS_LOG(INFO) << "Failed attempt to decode data.";
          std::move(on_decoding_failed).Run(data);
          return;
        }

        std::move(on_decoding_success).Run(*std::move(result));
      },
      std::move(on_decoding_success), std::move(on_decoding_failed), data);

  // Run the decoder
  base::BindOnce(decoder_method, decoder_, data, std::move(decoder_callback))
      .Run();
}

void Connection::OnConnectionClosed(
    TargetDeviceConnectionBroker::ConnectionClosedReason reason) {
  connection_state_ = Connection::State::kClosed;
  std::move(on_connection_closed_).Run(reason);
}

void Connection::OnResponseTimeout(QuickStartResponseType response_type) {
  // Ensures that if the response is received after this timeout but before the
  // Connection is destroyed, it will be ignored.
  response_weak_ptr_factory_.InvalidateWeakPtrs();

  QS_LOG(ERROR) << "Timed out waiting for " << response_type
                << " response from source device.";
  Close(TargetDeviceConnectionBroker::ConnectionClosedReason::kResponseTimeout);
  quick_start_metrics::RecordMessageReceived(
      /*desired_message_type=*/quick_start_metrics::MapResponseToMessageType(
          response_type),
      /*succeeded=*/false,
      /*listen_duration=*/kDefaultRoundTripTimeout,
      quick_start_metrics::MessageReceivedErrorCode::kTimeOut);
  message_elapsed_timer_.reset();
  if (response_type == QuickStartResponseType::kHandshake &&
      handshake_elapsed_timer_) {
    handshake_elapsed_timer_.reset();
  }
}

void Connection::OnResponseReceived(
    ConnectionResponseCallback callback,
    QuickStartResponseType response_type,
    absl::optional<std::vector<uint8_t>> response_bytes) {
  // Cancel the timeout timer if running.
  response_timeout_timer_.Stop();

  QS_LOG(INFO) << "Received " << response_type
               << " response from source device";

  if (!response_bytes.has_value()) {
    quick_start_metrics::RecordMessageReceived(
        /*desired_message_type=*/quick_start_metrics::MapResponseToMessageType(
            response_type),
        /*succeeded=*/false,
        /*listen_duration=*/message_elapsed_timer_->Elapsed(),
        quick_start_metrics::MessageReceivedErrorCode::kDeserializationFailure);
  } else {
    quick_start_metrics::RecordMessageReceived(
        /*desired_message_type=*/quick_start_metrics::MapResponseToMessageType(
            response_type),
        /*succeeded=*/true,
        /*listen_duration=*/message_elapsed_timer_->Elapsed(), absl::nullopt);
  }
  message_elapsed_timer_.reset();
  std::move(callback).Run(std::move(response_bytes));
}

}  // namespace ash::quick_start
