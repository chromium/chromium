// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/oobe_quick_start/connectivity/authenticated_connection.h"

#include "base/base64.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "chrome/browser/nearby_sharing/public/cpp/nearby_connection.h"
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

}  // namespace

AuthenticatedConnection::AuthenticatedConnection(
    NearbyConnection* nearby_connection)
    : Connection(nearby_connection) {}

AuthenticatedConnection::~AuthenticatedConnection() = default;

void AuthenticatedConnection::RequestAccountTransferAssertion(
    const std::string& challenge_b64url) {
  challenge_b64url_ = challenge_b64url;
  SendBootstrapOptions();
}

void AuthenticatedConnection::SendBootstrapOptions() {
  base::Value::Dict bootstrap_options;
  bootstrap_options.Set(kAccountRequirementKey, kAccountRequirementSingle);
  bootstrap_options.Set(kFlowTypeKey, kFlowTypeTargetChallenge);

  base::Value::Dict message_payload;
  message_payload.Set(kBootstrapOptionsKey, std::move(bootstrap_options));

  SendPayload(message_payload);
  nearby_connection_->Read(
      base::BindOnce(&AuthenticatedConnection::OnBootstrapOptionsResponse,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AuthenticatedConnection::GetInfo() {
  std::vector<uint8_t> ctap_request_command({kAuthenticatorGetInfoCommand});
  base::Value::Dict second_device_auth_payload;
  second_device_auth_payload.Set(kFidoMessageKey,
                                 base::Base64Encode(ctap_request_command));
  base::Value::Dict message_payload;
  message_payload.Set(kSecondDeviceAuthPayloadKey,
                      std::move(second_device_auth_payload));
  SendPayload(message_payload);
  nearby_connection_->Read(
      base::BindOnce(&AuthenticatedConnection::OnFidoGetInfoResponse,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AuthenticatedConnection::RequestAssertion() {
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
  nearby_connection_->Read(
      base::BindOnce(&AuthenticatedConnection::OnFidoGetAssertionResponse,
                     weak_ptr_factory_.GetWeakPtr()));
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

void AuthenticatedConnection::OnBootstrapOptionsResponse(
    absl::optional<std::vector<uint8_t>>) {
  GetInfo();
}

void AuthenticatedConnection::OnFidoGetInfoResponse(
    absl::optional<std::vector<uint8_t>>) {
  RequestAssertion();
}

void AuthenticatedConnection::OnFidoGetAssertionResponse(
    absl::optional<std::vector<uint8_t>>) {
  NOTIMPLEMENTED();
}

}  // namespace ash::quick_start
