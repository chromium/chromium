// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/oobe_quick_start/connectivity/connection.h"

#include <array>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/json_writer.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/fido_assertion_info.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/handshake_helpers.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/session_context.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/target_device_connection_broker.h"
#include "chromeos/ash/components/nearby/common/connections_manager/nearby_connection.h"
#include "chromeos/ash/components/quick_start/logging.h"
#include "chromeos/ash/components/quick_start/quick_start_message.h"
#include "chromeos/ash/components/quick_start/quick_start_metrics.h"
#include "chromeos/ash/components/quick_start/quick_start_requests.h"
#include "chromeos/ash/components/quick_start/types.h"
#include "chromeos/ash/services/nearby/public/mojom/quick_start_decoder.mojom.h"
#include "chromeos/ash/services/nearby/public/mojom/quick_start_decoder_types.mojom-forward.h"
#include "chromeos/ash/services/nearby/public/mojom/quick_start_decoder_types.mojom-shared.h"
#include "chromeos/ash/services/nearby/public/mojom/quick_start_decoder_types.mojom.h"

namespace ash::quick_start {

namespace {

std::string MessageTagToString(mojom::QuickStartMessage::Tag tag) {
  switch (tag) {
    case mojom::QuickStartMessage::Tag::kBootstrapConfigurations:
      return "BootstrapConfigurations";
    case mojom::QuickStartMessage::Tag::kWifiCredentials:
      return "WifiCredentials";
    case mojom::QuickStartMessage::Tag::kNotifySourceOfUpdateResponse:
      return "NotifySourceOfUpdateResponse";
    case mojom::QuickStartMessage::Tag::kFidoAssertionResponse:
      return "FidoAssertionResponse";
    case mojom::QuickStartMessage::Tag::kUserVerificationRequested:
      return "UserVerificationRequested";
    case mojom::QuickStartMessage::Tag::kUserVerificationMethod:
      return "UserVerificationMethod";
    case mojom::QuickStartMessage::Tag::kUserVerificationResponse:
      return "UserVerificationResponse";
  }
}

}  // namespace

Connection::Factory::~Factory() = default;

std::unique_ptr<Connection> Connection::Factory::Create(
    NearbyConnection* nearby_connection,
    SessionContext* session_context,
    mojo::SharedRemote<mojom::QuickStartDecoder> quick_start_decoder,
    ConnectionClosedCallback on_connection_closed,
    ConnectionAuthenticatedCallback on_connection_authenticated) {
  return std::make_unique<Connection>(
      nearby_connection, session_context, std::move(quick_start_decoder),
      std::move(on_connection_closed), std::move(on_connection_authenticated));
}

Connection::Connection(
    NearbyConnection* nearby_connection,
    SessionContext* session_context,
    mojo::SharedRemote<mojom::QuickStartDecoder> quick_start_decoder,
    ConnectionClosedCallback on_connection_closed,
    ConnectionAuthenticatedCallback on_connection_authenticated)
    : nearby_connection_(nearby_connection),
      session_context_(session_context),
      on_connection_closed_(std::move(on_connection_closed)),
      on_connection_authenticated_(std::move(on_connection_authenticated)),
      decoder_(std::move(quick_start_decoder)) {
  quick_start_metrics_ = std::make_unique<QuickStartMetrics>();
  // Since we aren't expecting any disconnections, treat any drops of the
  // connection as an unknown error.
  nearby_connection->SetDisconnectionListener(base::BindOnce(
      &Connection::OnConnectionClosed, weak_ptr_factory_.GetWeakPtr(),
      TargetDeviceConnectionBroker::ConnectionClosedReason::kUnknownError));
}

Connection::~Connection() = default;

void Connection::MarkConnectionAuthenticated(
    QuickStartMetrics::AuthenticationMethod auth_method) {
  authenticated_ = true;
  if (on_connection_authenticated_) {
    std::move(on_connection_authenticated_).Run(weak_ptr_factory_.GetWeakPtr());
  }
  QuickStartMetrics::RecordAuthenticationMethod(auth_method);
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

  if (authenticated_ &&
      reason ==
          TargetDeviceConnectionBroker::ConnectionClosedReason::kUserAborted) {
    // TODO(b/306422046): Verify the message is received despite closing the
    // NearbyConnection immediately after.
    SendMessageWithoutResponse(requests::BuildBootstrapStateCancelMessage(),
                               QuickStartResponseType::kBootstrapStateCancel);
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
  SessionContext::SharedSecret secondary_shared_secret =
      session_context_->secondary_shared_secret();
  std::string shared_secret_str(secondary_shared_secret.begin(),
                                secondary_shared_secret.end());
  auto convert_result = base::BindOnce(
      [](RequestWifiCredentialsCallback callback,
         mojom::QuickStartMessagePtr wifi_credentials) {
        if (!wifi_credentials || !wifi_credentials->is_wifi_credentials()) {
          std::move(callback).Run(std::nullopt);
          return;
        }
        std::move(callback).Run(
            std::move(*wifi_credentials->get_wifi_credentials()));
      },
      std::move(callback));
  SendMessageAndDecodeResponse(
      requests::BuildRequestWifiCredentialsMessage(
          session_context_->session_id(), shared_secret_str),
      QuickStartResponseType::kWifiCredentials, std::move(convert_result));
}

void Connection::NotifySourceOfUpdate(NotifySourceOfUpdateCallback callback) {
  SessionContext::SharedSecret secondary_shared_secret =
      session_context_->secondary_shared_secret();
  SendMessageAndDecodeResponse(
      requests::BuildNotifySourceOfUpdateMessage(session_context_->session_id(),
                                                 secondary_shared_secret),
      QuickStartResponseType::kNotifySourceOfUpdate,
      base::BindOnce(&Connection::OnNotifySourceOfUpdateResponse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void Connection::RequestAccountInfo(RequestAccountInfoCallback callback) {
  // During this roundtrip the source device may prompt the user to select an
  // account before responding, so we will provide a very generous timeout. (In
  // case, for example, the user has walked away after the verification step.)
  constexpr base::TimeDelta timeout = base::Minutes(30);
  SendMessageAndDecodeResponse(
      requests::BuildBootstrapOptionsRequest(),
      QuickStartResponseType::kBootstrapConfigurations,
      base::BindOnce(&Connection::OnBootstrapConfigurationsResponse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
      timeout);
}

void Connection::RequestAccountTransferAssertion(
    const Base64UrlString& challenge,
    RequestAccountTransferAssertionCallback callback) {
  client_data_ = std::make_unique<AccountTransferClientData>(challenge);

  auto request_assertion = base::BindOnce(
      &Connection::SendMessageAndDecodeResponse, weak_ptr_factory_.GetWeakPtr(),
      requests::BuildAssertionRequestMessage(client_data_->CreateHash()),
      QuickStartResponseType::kAssertion,
      base::BindOnce(&Connection::OnRequestAccountTransferAssertionResponse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
      kDefaultRoundTripTimeout);

  // We can discard the GetInfo response since we assume that all
  // Quick-Start-compatible Android devices have valid CTAP2 functionality. If
  // GetInfo produced an error on the source device, we will likely run into the
  // same error on the GetAssertion step.
  SendMessageAndDiscardResponse(requests::BuildGetInfoRequestMessage(),
                                QuickStartResponseType::kGetInfo,
                                std::move(request_assertion));
}

void Connection::OnNotifySourceOfUpdateResponse(
    NotifySourceOfUpdateCallback callback,
    mojom::QuickStartMessagePtr quick_start_message) {
  response_timeout_timer_.Stop();

  if (!quick_start_message) {
    QS_LOG(ERROR) << "Failed to decode NotifySourceOfUpdateResponse";
    std::move(callback).Run(/*ack_received=*/false);
    return;
  }

  if (!quick_start_message->is_notify_source_of_update_response()) {
    QS_LOG(ERROR) << "Unexpected message: received="
                  << MessageTagToString(quick_start_message->which())
                  << ", expected=NotifySourceOfUpdateResponse";
    std::move(callback).Run(/*ack_successful=*/false);
    return;
  }

  if (!quick_start_message->get_notify_source_of_update_response()
           ->ack_received) {
    QS_LOG(ERROR) << "The ack received value in the NotifySourceOfUpdate "
                     "response is unexpectedly 'false'.";
    std::move(callback).Run(/*ack_successful=*/false);
    return;
  }

  std::move(callback).Run(/*ack_successful=*/true);
}

void Connection::OnRequestAccountTransferAssertionResponse(
    RequestAccountTransferAssertionCallback callback,
    mojom::QuickStartMessagePtr quick_start_message) {
  if (!quick_start_message ||
      !quick_start_message->is_fido_assertion_response()) {
    // TODO (b/286877412): Update this logic once we've aligned on an unknown
    // message strategy.
    QS_LOG(INFO) << "Ignoring message and re-reading";
    nearby_connection_->Read(base::BindOnce(
        &Connection::DecodeQuickStartMessage, weak_ptr_factory_.GetWeakPtr(),
        base::BindOnce(&Connection::OnRequestAccountTransferAssertionResponse,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback))));
    return;
  }

  mojom::FidoAssertionResponsePtr& fido_response =
      quick_start_message->get_fido_assertion_response();
  FidoAssertionInfo assertion_info;
  assertion_info.email = fido_response->email;

  // The credential_id response is sent to us as raw bytes, Base64Url encode
  // them.
  assertion_info.credential_id = Base64UrlEncode(fido_response->credential_id);

  assertion_info.authenticator_data = fido_response->auth_data;
  assertion_info.signature = fido_response->signature;
  std::string client_data = client_data_->CreateJson();
  assertion_info.client_data =
      std::vector<uint8_t>(client_data.begin(), client_data.end());

  std::move(callback).Run(assertion_info);
}

void Connection::OnBootstrapConfigurationsResponse(
    RequestAccountInfoCallback callback,
    mojom::QuickStartMessagePtr quick_start_message) {
  if (!quick_start_message ||
      !quick_start_message->is_bootstrap_configurations()) {
    std::move(callback).Run("");
    return;
  }
  phone_instance_id_ =
      quick_start_message->get_bootstrap_configurations()->instance_id;
  is_supervised_account_ = quick_start_message->get_bootstrap_configurations()
                               ->is_supervised_account;
  std::move(callback).Run(
      quick_start_message->get_bootstrap_configurations()->email);
}

void Connection::SendMessageAndDecodeResponse(
    std::unique_ptr<QuickStartMessage> message,
    QuickStartResponseType response_type,
    OnDecodingCompleteCallback callback,
    base::TimeDelta timeout) {
  std::string json_serialized_payload;
  CHECK(base::JSONWriter::Write(*message->GenerateEncodedMessage(),
                                &json_serialized_payload))
      << "Failed to write JSON.";

  SendBytesAndReadResponse(
      std::vector<uint8_t>(json_serialized_payload.begin(),
                           json_serialized_payload.end()),
      response_type,
      base::BindOnce(&Connection::DecodeQuickStartMessage,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
      timeout);
}

void Connection::SendMessageAndDiscardResponse(
    std::unique_ptr<QuickStartMessage> message,
    QuickStartResponseType response_type,
    base::OnceClosure callback,
    base::TimeDelta timeout) {
  std::string json_serialized_payload;
  CHECK(base::JSONWriter::Write(*message->GenerateEncodedMessage(),
                                &json_serialized_payload))
      << "Failed to write JSON.";

  SendBytesAndReadResponse(
      std::vector<uint8_t>(json_serialized_payload.begin(),
                           json_serialized_payload.end()),
      response_type,
      base::IgnoreArgs<std::optional<std::vector<uint8_t>>>(
          std::move(callback)),
      timeout);
}

void Connection::SendMessageWithoutResponse(
    std::unique_ptr<QuickStartMessage> message,
    QuickStartResponseType message_type) {
  std::string json_serialized_payload;
  CHECK(base::JSONWriter::Write(*message->GenerateEncodedMessage(),
                                &json_serialized_payload))
      << "Failed to write JSON.";
  quick_start_metrics_->RecordMessageSent(
      QuickStartMetrics::MapResponseToMessageType(message_type));
  nearby_connection_->Write(std::vector<uint8_t>(
      json_serialized_payload.begin(), json_serialized_payload.end()));
}

void Connection::SendBytesAndReadResponse(std::vector<uint8_t>&& bytes,
                                          QuickStartResponseType response_type,
                                          ConnectionResponseCallback callback,
                                          base::TimeDelta timeout) {
  quick_start_metrics_->RecordMessageSent(
      QuickStartMetrics::MapResponseToMessageType(response_type));
  nearby_connection_->Write(std::move(bytes));
  nearby_connection_->Read(base::BindOnce(
      &Connection::OnResponseReceived, response_weak_ptr_factory_.GetWeakPtr(),
      std::move(callback), response_type));

  response_timeout_timer_.Start(
      FROM_HERE, timeout,
      base::BindOnce(&Connection::OnResponseTimeout,
                     weak_ptr_factory_.GetWeakPtr(), response_type));
}

void Connection::InitiateHandshake(const std::string& authentication_token,
                                   HandshakeSuccessCallback callback) {
  SendBytesAndReadResponse(
      handshake::BuildHandshakeMessage(authentication_token,
                                       session_context_->shared_secret()),
      QuickStartResponseType::kHandshake,
      base::BindOnce(&Connection::OnHandshakeResponse,
                     weak_ptr_factory_.GetWeakPtr(), authentication_token,
                     std::move(callback)));
  quick_start_metrics_->RecordHandshakeStarted();
}

void Connection::OnHandshakeResponse(
    const std::string& authentication_token,
    HandshakeSuccessCallback callback,
    std::optional<std::vector<uint8_t>> response_bytes) {
  if (!response_bytes) {
    QS_LOG(ERROR) << "Failed to read handshake response from NearbyConnection";
    quick_start_metrics_->RecordHandshakeResult(
        /*success=*/false,
        /*error_code=*/
        QuickStartMetrics::HandshakeErrorCode::kFailedToReadResponse);
    std::move(callback).Run(/*success=*/false);
    return;
  }
  handshake::VerifyHandshakeMessageStatus status =
      handshake::VerifyHandshakeMessage(*response_bytes, authentication_token,
                                        session_context_->shared_secret());
  bool success = status == handshake::VerifyHandshakeMessageStatus::kSuccess;
  quick_start_metrics_->RecordHandshakeResult(
      /*success=*/success, handshake::MapHandshakeStatusToErrorCode(status));
  std::move(callback).Run(success);
}

void Connection::WaitForUserVerification(
    AwaitUserVerificationCallback callback) {
  DoWaitForUserVerification(/*attempt_number=*/1, std::move(callback));
}

void Connection::DoWaitForUserVerification(
    size_t attempt_number,
    AwaitUserVerificationCallback callback) {
  // The source device may skip the UserVerificationRequested and
  // UserVerificationMethod packets, but we should always get the
  // UserVerificationResponse within three packets.
  if (attempt_number > 3) {
    QS_LOG(ERROR) << "Unexpected number of user verification packets.";
    std::move(callback).Run(std::nullopt);
    return;
  }

  nearby_connection_->Read(base::BindOnce(
      &Connection::DecodeQuickStartMessage, weak_ptr_factory_.GetWeakPtr(),
      base::BindOnce(&Connection::OnUserVerificationPacketDecoded,
                     weak_ptr_factory_.GetWeakPtr(), attempt_number,
                     std::move(callback))));
}

void Connection::OnUserVerificationPacketDecoded(
    size_t attempt_number,
    AwaitUserVerificationCallback callback,
    mojom::QuickStartMessagePtr quick_start_message) {
  if (!quick_start_message) {
    QS_LOG(ERROR) << "Failed to decode Quick Start message";
    std::move(callback).Run(std::nullopt);
    return;
  }
  switch (quick_start_message->which()) {
    case mojom::QuickStartMessage::Tag::kUserVerificationRequested:
      if (!quick_start_message->get_user_verification_requested()
               ->is_awaiting_user_verification) {
        QS_LOG(ERROR) << "User verification request received from phone, but "
                         "is_awaiting_user_verification is false.";
        std::move(callback).Run(std::nullopt);
        return;
      }
      DoWaitForUserVerification(attempt_number + 1, std::move(callback));
      return;
    case mojom::QuickStartMessage::Tag::kUserVerificationMethod:
      if (!quick_start_message->get_user_verification_method()
               ->use_source_lock_screen_prompt) {
        QS_LOG(ERROR) << "Unsupported user verification method";
        std::move(callback).Run(std::nullopt);
        return;
      }
      DoWaitForUserVerification(attempt_number + 1, std::move(callback));
      return;
    case mojom::QuickStartMessage::Tag::kUserVerificationResponse:
      std::move(callback).Run(
          quick_start_message->get_user_verification_response()
              ? std::make_optional<mojom::UserVerificationResponse>(std::move(
                    *quick_start_message->get_user_verification_response()))
              : std::nullopt);
      return;
    default:
      QS_LOG(ERROR) << "Unexpected message: received a packet of type "
                    << MessageTagToString(quick_start_message->which())
                    << " during user verification.";
      std::move(callback).Run(std::nullopt);
      return;
  }
}

base::Value::Dict Connection::GetPrepareForUpdateInfo() {
  return session_context_->GetPrepareForUpdateInfo();
}

void Connection::NotifyPhoneSetupComplete() {
  SendMessageWithoutResponse(requests::BuildBootstrapStateCompleteMessage(),
                             QuickStartResponseType::kBootstrapStateComplete);
}

void Connection::DecodeQuickStartMessage(
    OnDecodingCompleteCallback on_decoding_complete,
    std::optional<std::vector<uint8_t>> data) {
  if (!data || data->empty()) {
    QS_LOG(INFO) << "Empty response";
    std::move(on_decoding_complete).Run(nullptr);
    return;
  }

  // Setup a callback to handle the decoder's response. If an error was
  // reported, return empty. If not, run the success callback with the
  // decoded data.
  mojom::QuickStartDecoder::DecodeQuickStartMessageCallback decoder_callback =
      base::BindOnce(
          [](OnDecodingCompleteCallback on_decoding_complete,
             mojom::QuickStartMessagePtr data,
             std::optional<mojom::QuickStartDecoderError> error) {
            if (error.has_value() || !data) {
              // TODO(b/281052191): Log error code here
              QS_LOG(ERROR) << "Error decoding data.";
              std::move(on_decoding_complete).Run(nullptr);
              return;
            }

            std::move(on_decoding_complete).Run(std::move(data));
          },
          std::move(on_decoding_complete));

  // Run the decoder
  decoder_->DecodeQuickStartMessage(data, std::move(decoder_callback));
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
  quick_start_metrics_->RecordMessageReceived(
      /*desired_message_type=*/QuickStartMetrics::MapResponseToMessageType(
          response_type),
      /*succeeded=*/false,
      QuickStartMetrics::MessageReceivedErrorCode::kTimeOut);
}

void Connection::OnResponseReceived(
    ConnectionResponseCallback callback,
    QuickStartResponseType response_type,
    std::optional<std::vector<uint8_t>> response_bytes) {
  // Cancel the timeout timer if running.
  response_timeout_timer_.Stop();

  QS_LOG(INFO) << "Received " << response_type
               << " response from source device";

  if (!response_bytes.has_value()) {
    quick_start_metrics_->RecordMessageReceived(
        /*desired_message_type=*/QuickStartMetrics::MapResponseToMessageType(
            response_type),
        /*succeeded=*/false,
        QuickStartMetrics::MessageReceivedErrorCode::kDeserializationFailure);
  } else {
    quick_start_metrics_->RecordMessageReceived(
        /*desired_message_type=*/QuickStartMetrics::MapResponseToMessageType(
            response_type),
        /*succeeded=*/true, std::nullopt);
  }

  // NearbyConnection will invoke its read callback if there is one pending when
  // it is destroyed. In these instances we didn't actually receive a response
  // so we need to ensure the connection is still open before invoking
  // |callback| here.
  if (connection_state_ == Connection::State::kOpen) {
    std::move(callback).Run(std::move(response_bytes));
  }
}

}  // namespace ash::quick_start
