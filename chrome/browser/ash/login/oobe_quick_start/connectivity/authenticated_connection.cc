// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/oobe_quick_start/connectivity/authenticated_connection.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/fido_assertion_info.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/random_session_id.h"
#include "chrome/browser/ash/login/oobe_quick_start/logging/logging.h"
#include "chrome/browser/nearby_sharing/public/cpp/nearby_connection.h"
#include "chromeos/ash/components/quick_start/quick_start_message.h"
#include "chromeos/ash/components/quick_start/quick_start_requests.h"
#include "chromeos/ash/services/nearby/public/mojom/quick_start_decoder.mojom.h"
#include "chromeos/ash/services/nearby/public/mojom/quick_start_decoder_types.mojom.h"

namespace ash::quick_start {

namespace {

const char kNotifySourceOfUpdateMessageKey[] = "isForcedUpdateRequired";

}  // namespace

AuthenticatedConnection::AuthenticatedConnection(
    NearbyConnection* nearby_connection,
    mojo::SharedRemote<mojom::QuickStartDecoder> remote,
    RandomSessionId session_id,
    SharedSecret shared_secret)
    : Connection(nearby_connection, session_id, shared_secret) {
  decoder_ = std::move(remote);
}

AuthenticatedConnection::~AuthenticatedConnection() = default;

void AuthenticatedConnection::RequestAccountTransferAssertion(
    const std::string& challenge_b64url,
    RequestAccountTransferAssertionCallback callback) {
  auto parse_assertion_response =
      base::BindOnce(&AuthenticatedConnection::ParseAssertionResponse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback));

  auto request_assertion =
      base::IgnoreArgs<absl::optional<std::vector<uint8_t>>>(base::BindOnce(
          &AuthenticatedConnection::SendMessage, weak_ptr_factory_.GetWeakPtr(),
          requests::BuildAssertionRequestMessage(challenge_b64url),
          std::move(parse_assertion_response)));

  // Set up a callback to call GetInfo, calling back into RequestAssertion (and
  // ignoring the results of GetInfo) after the call succeeds.
  auto get_info =
      base::IgnoreArgs<absl::optional<std::vector<uint8_t>>>(base::BindOnce(
          &AuthenticatedConnection::SendMessage, weak_ptr_factory_.GetWeakPtr(),
          requests::BuildGetInfoRequestMessage(),
          std::move(request_assertion)));

  // Call into SetBootstrapOptions, starting the chain of callbacks.
  SendMessage(requests::BuildBootstrapOptionsRequest(), std::move(get_info));
}

void AuthenticatedConnection::NotifySourceOfUpdate() {
  base::Value::Dict message_payload;
  message_payload.Set(kNotifySourceOfUpdateMessageKey, true);
  SendPayload(message_payload);
}

void AuthenticatedConnection::RequestWifiCredentials(
    int32_t session_id,
    RequestWifiCredentialsCallback callback) {
  // Build the Wifi Credential Request payload
  std::string shared_secret_str(shared_secret_.begin(), shared_secret_.end());
  SendMessage(
      requests::BuildRequestWifiCredentialsMessage(session_id,
                                                   shared_secret_str),
      base::BindOnce(&AuthenticatedConnection::ParseWifiCredentialsResponse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void AuthenticatedConnection::ParseWifiCredentialsResponse(
    RequestWifiCredentialsCallback callback,
    absl::optional<std::vector<uint8_t>> response_bytes) {
  // TODO (b/234655072): Implement response parsing.
  NOTIMPLEMENTED();
}

void AuthenticatedConnection::ParseAssertionResponse(
    RequestAccountTransferAssertionCallback callback,
    absl::optional<std::vector<uint8_t>> response_bytes) {
  if (!response_bytes.has_value()) {
    std::move(callback).Run(absl::nullopt);
    return;
  }

  auto parse_mojo_response_callback =
      base::BindOnce(&AuthenticatedConnection::GenerateFidoAssertionInfo,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback));

  decoder_->DecodeGetAssertionResponse(response_bytes.value(),
                                       std::move(parse_mojo_response_callback));
}

void AuthenticatedConnection::GenerateFidoAssertionInfo(
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

void AuthenticatedConnection::SendMessage(
    std::unique_ptr<QuickStartMessage> message,
    ConnectionResponseCallback callback) {
  SendPayload(*message->GenerateEncodedMessage());
  nearby_connection_->Read(std::move(callback));
}

}  // namespace ash::quick_start
