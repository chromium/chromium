// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/oobe_quick_start/connectivity/authenticated_connection.h"

#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/fido_assertion_info.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/random_session_id.h"
#include "chrome/browser/ash/login/oobe_quick_start/logging/logging.h"
#include "chrome/browser/nearby_sharing/public/cpp/nearby_connection.h"
#include "chromeos/ash/services/nearby/public/mojom/quick_start_decoder.mojom.h"
#include "chromeos/ash/services/nearby/public/mojom/quick_start_decoder_types.mojom.h"
#include "components/cbor/values.h"
#include "components/cbor/writer.h"
#include "crypto/sha2.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace ash::quick_start {

namespace {

// MessagePayload key telling the phone specific options
// for how to handle account transfer and fallback.
constexpr char kBootstrapOptionsKey[] = "bootstrapOptions";

// bootstrapOptions key telling the phone the number of
// accounts are expected to transfer account to the target device.
constexpr char kAccountRequirementKey[] = "accountRequirement";

// bootstrapOptions key telling the phone how to handle
// challenge UI in case of fallback.
constexpr char kFlowTypeKey[] = "flowType";

// MessagePayload key providing account transfer request for target device.
constexpr char kSecondDeviceAuthPayloadKey[] = "secondDeviceAuthPayload";

// Base64 encoded CBOR bytes containing the Fido command. This will be used for
// GetInfo and GetAssertion.
constexpr char kFidoMessageKey[] = "fidoMessage";

// Wrapper around Quick Start Payloads
constexpr char kQuickStartPayload[] = "quickStartPayload";

// Boolean in WifiCredentialsRequest indicating we should request WiFi
// Credentials
constexpr char kRequestWifiKey[] = "request_wifi";

// Key in WifiCredentialsRequest including the shared secret
constexpr char kSharedSecretKey[] = "shared_secret";

// Key in WifiCredentialsRequest for the session ID
constexpr char kSessionIdKey[] = "SESSION_ID";

// Maps to AccountRequirementSingle enum value for Account Requirement field
// meaning that at least one account is required on the phone. The user will
// select the specified account to transfer.
// Enum Source: go/bootstrap-options-account-requirement-single.
constexpr int kAccountRequirementSingle = 2;

// Maps to FlowTypeTargetChallenge enum value for Flow Type field meaning that
// the fallback challenge will happen on the target device.
// Enum Source: go/bootstrap-options-flow-type-target-challenge.
constexpr int kFlowTypeTargetChallenge = 2;

const char kRelyingPartyId[] = "google.com";
const char kOrigin[] = "https://accounts.google.com";
const char kCtapRequestType[] = "webauthn.get";

// Maps to CBOR byte labelling FIDO request as GetInfo.
const uint8_t kAuthenticatorGetInfoCommand = 0x04;

// Maps to CBOR byte labelling FIDO request as GetAssertion.
const uint8_t kAuthenticatorGetAssertionCommand = 0x02;
const char kUserPresenceMapKey[] = "up";
const char kUserVerificationMapKey[] = "uv";

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
  challenge_b64url_ = challenge_b64url;

  auto parse_assertion_response =
      base::BindOnce(&AuthenticatedConnection::ParseAssertionResponse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback));

  auto request_assertion =
      base::IgnoreArgs<absl::optional<std::vector<uint8_t>>>(base::BindOnce(
          &AuthenticatedConnection::RequestAssertion,
          weak_ptr_factory_.GetWeakPtr(), std::move(parse_assertion_response)));

  // Set up a callback to call GetInfo, calling back into RequestAssertion (and
  // ignoring the results of GetInfo) after the call succeeds.
  auto get_info =
      base::IgnoreArgs<absl::optional<std::vector<uint8_t>>>(base::BindOnce(
          &AuthenticatedConnection::GetInfo, weak_ptr_factory_.GetWeakPtr(),
          std::move(request_assertion)));

  // Call into SetBootstrapOptions, starting the chain of callbacks.
  SendBootstrapOptions(std::move(get_info));
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
  base::Value::Dict wifi_credential_request_payload;
  wifi_credential_request_payload.Set(kRequestWifiKey, true);
  std::string shared_secret_str(shared_secret_.begin(), shared_secret_.end());
  std::string shared_secret_base64;
  base::Base64Encode(shared_secret_str, &shared_secret_base64);

  wifi_credential_request_payload.Set(kSessionIdKey, session_id);

  // TODO (b/234655072): Create a new Shared Secret and persist for Forced
  // Update
  wifi_credential_request_payload.Set(kSharedSecretKey, shared_secret_base64);

  // Encode above payload in a Base64 String
  std::string wifi_credential_request_payload_json;
  bool json_writer_succeeded = base::JSONWriter::Write(
      wifi_credential_request_payload, &wifi_credential_request_payload_json);

  if (!json_writer_succeeded) {
    QS_LOG(ERROR) << "Failed to create Wifi Request payload";
    return;
  }

  std::string wifi_credential_request_payload_base64;
  base::Base64Encode(wifi_credential_request_payload_json,
                     &wifi_credential_request_payload_base64);

  // Build a QuickStartPayload, with the above Base64 encoded data.
  base::Value::Dict message_payload;
  message_payload.Set(kQuickStartPayload,
                      std::move(wifi_credential_request_payload_base64));
  SendPayload(message_payload);

  nearby_connection_->Read(
      base::BindOnce(&AuthenticatedConnection::ParseWifiCredentialsResponse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void AuthenticatedConnection::ParseWifiCredentialsResponse(
    RequestWifiCredentialsCallback callback,
    absl::optional<std::vector<uint8_t>> response_bytes) {
  // TODO (b/234655072): Implement response parsing.
  NOTIMPLEMENTED();
}

void AuthenticatedConnection::SendBootstrapOptions(
    ConnectionResponseCallback callback) {
  base::Value::Dict bootstrap_options;
  bootstrap_options.Set(kAccountRequirementKey, kAccountRequirementSingle);
  bootstrap_options.Set(kFlowTypeKey, kFlowTypeTargetChallenge);

  base::Value::Dict message_payload;
  message_payload.Set(kBootstrapOptionsKey, std::move(bootstrap_options));

  SendPayload(message_payload);
  nearby_connection_->Read(std::move(callback));
}

void AuthenticatedConnection::GetInfo(ConnectionResponseCallback callback) {
  std::vector<uint8_t> ctap_request_command({kAuthenticatorGetInfoCommand});
  base::Value::Dict second_device_auth_payload;
  second_device_auth_payload.Set(kFidoMessageKey,
                                 base::Base64Encode(ctap_request_command));
  base::Value::Dict message_payload;
  message_payload.Set(kSecondDeviceAuthPayloadKey,
                      std::move(second_device_auth_payload));
  SendPayload(message_payload);
  nearby_connection_->Read(std::move(callback));
}

void AuthenticatedConnection::RequestAssertion(
    ConnectionResponseCallback callback) {
  DCHECK(!challenge_b64url_.empty());
  cbor::Value request = GenerateGetAssertionRequest();
  std::vector<uint8_t> ctap_request_command =
      CBOREncodeGetAssertionRequest(std::move(request));
  base::Value::Dict second_device_auth_payload;
  second_device_auth_payload.Set(kFidoMessageKey,
                                 base::Base64Encode(ctap_request_command));
  base::Value::Dict message_payload;
  message_payload.Set(kSecondDeviceAuthPayloadKey,
                      std::move(second_device_auth_payload));
  SendPayload(message_payload);
  nearby_connection_->Read(std::move(callback));
}

cbor::Value AuthenticatedConnection::GenerateGetAssertionRequest() {
  url::Origin origin = url::Origin::Create(GURL(kOrigin));
  std::string client_data_json = CreateFidoClientDataJson(origin);
  cbor::Value::MapValue cbor_map;
  cbor_map.insert_or_assign(cbor::Value(0x01), cbor::Value(kRelyingPartyId));
  std::array<uint8_t, crypto::kSHA256Length> client_data_hash;
  crypto::SHA256HashString(client_data_json, client_data_hash.data(),
                           client_data_hash.size());
  cbor_map.insert_or_assign(cbor::Value(0x02), cbor::Value(client_data_hash));
  cbor::Value::MapValue option_map;
  option_map.insert_or_assign(cbor::Value(kUserPresenceMapKey),
                              cbor::Value(true));
  option_map.insert_or_assign(cbor::Value(kUserVerificationMapKey),
                              cbor::Value(true));
  cbor_map.insert_or_assign(cbor::Value(0x05),
                            cbor::Value(std::move(option_map)));
  return cbor::Value(std::move(cbor_map));
}

std::vector<uint8_t> AuthenticatedConnection::CBOREncodeGetAssertionRequest(
    const cbor::Value& request) {
  // Encode the CtapGetAssertionRequest into cbor bytes vector.
  absl::optional<std::vector<uint8_t>> cbor_bytes =
      cbor::Writer::Write(request);
  DCHECK(cbor_bytes);
  std::vector<uint8_t> request_bytes = std::move(*cbor_bytes);
  // Add the command byte to the beginning of this now fully encoded cbor bytes
  // vector.
  request_bytes.insert(request_bytes.begin(),
                       kAuthenticatorGetAssertionCommand);
  return request_bytes;
}

std::string AuthenticatedConnection::CreateFidoClientDataJson(
    const url::Origin& origin) {
  base::Value::Dict fido_collected_client_data;
  fido_collected_client_data.Set("type", kCtapRequestType);
  fido_collected_client_data.Set("challenge", challenge_b64url_);
  fido_collected_client_data.Set("origin", origin.Serialize());
  fido_collected_client_data.Set("crossOrigin", false);
  std::string fido_client_data_json;
  base::JSONWriter::Write(fido_collected_client_data, &fido_client_data_json);
  return fido_client_data_json;
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

}  // namespace ash::quick_start
