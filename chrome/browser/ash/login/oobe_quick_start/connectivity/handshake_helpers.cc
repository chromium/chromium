// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/oobe_quick_start/connectivity/handshake_helpers.h"

#include "base/containers/span.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/proto/aes_gcm_authentication_message.pb.h"
#include "chromeos/ash/components/quick_start/logging.h"
#include "crypto/aead.h"
#include "crypto/random.h"

namespace ash::quick_start::handshake {

namespace {

std::vector<uint8_t> EncryptPayload(
    const proto::V1Message::AuthenticationPayload& payload,
    base::span<const uint8_t> secret,
    base::span<const uint8_t> nonce) {
  std::string payload_bytes;
  payload.SerializeToString(&payload_bytes);

  crypto::Aead aead(crypto::Aead::AES_256_GCM);
  aead.Init(secret);
  return aead.Seal(
      std::vector<uint8_t>(payload_bytes.begin(), payload_bytes.end()), nonce,
      /*additional_data=*/base::span<uint8_t>());
}

std::optional<std::vector<uint8_t>> DecryptPayload(
    base::span<const uint8_t> payload,
    base::span<const uint8_t> secret,
    base::span<const uint8_t> nonce) {
  crypto::Aead aead(crypto::Aead::AES_256_GCM);
  aead.Init(secret);
  return aead.Open(payload, nonce,
                   /*additional_data=*/base::span<uint8_t>());
}

std::optional<proto::V1Message> ParseAuthMessage(
    base::span<const uint8_t> auth_message_bytes) {
  proto::AesGcmAuthenticationMessage auth_message;
  if (!auth_message.ParseFromString(
          std::string(auth_message_bytes.begin(), auth_message_bytes.end()))) {
    QS_LOG(ERROR) << "Failed to parse AesGcmAuthenticationMessage.";
    return std::nullopt;
  }
  if (!auth_message.has_version() || auth_message.version() != 1 ||
      !auth_message.has_v1()) {
    QS_LOG(ERROR) << "AesGcmAuthenticationMessage has unknown version.";
    return std::nullopt;
  }
  const proto::V1Message& v1_message = auth_message.v1();
  if (!v1_message.has_nonce()) {
    QS_LOG(ERROR) << "AesGcmAuthenticationMessage missing nonce.";
    return std::nullopt;
  }
  if (v1_message.nonce().size() != 12u) {
    QS_LOG(ERROR) << "Nonce has bad length. Should be 12 bytes.";
    return std::nullopt;
  }
  if (!v1_message.has_payload()) {
    QS_LOG(ERROR) << "AesGcmAuthenticationMessage missing payload.";
    return std::nullopt;
  }
  return v1_message;
}

std::optional<proto::V1Message::AuthenticationPayload> ParseAuthPayload(
    base::span<const uint8_t> bytes) {
  proto::V1Message::AuthenticationPayload auth_payload;
  if (!auth_payload.ParseFromString(std::string(bytes.begin(), bytes.end()))) {
    QS_LOG(ERROR) << "Failed to parse AuthenticationPayload.";
    return std::nullopt;
  }
  if (!auth_payload.has_role()) {
    QS_LOG(ERROR) << "AuthenticationPayload missing role.";
    return std::nullopt;
  }

  if (!auth_payload.has_auth_string()) {
    QS_LOG(ERROR) << "AuthenticationPayload missing auth_string.";
    return std::nullopt;
  }
  return auth_payload;
}

}  // namespace

std::vector<uint8_t> BuildHandshakeMessage(
    const std::string& auth_token,
    std::array<uint8_t, 32> secret,
    std::optional<std::array<uint8_t, 12>> nonce,
    DeviceRole role) {
  std::array<uint8_t, 12> nonce_bytes;
  if (nonce) {
    nonce_bytes = *nonce;
  } else {
    crypto::RandBytes(nonce_bytes);
  }

  proto::V1Message::AuthenticationPayload auth_payload;
  auth_payload.set_role(static_cast<int32_t>(role));
  auth_payload.set_auth_string(auth_token);

  std::vector<uint8_t> encrypted_payload =
      EncryptPayload(auth_payload, secret, nonce_bytes);

  proto::AesGcmAuthenticationMessage auth_message;
  auth_message.set_version(proto::AesGcmAuthenticationMessage::V1);
  proto::V1Message* v1_message = auth_message.mutable_v1();
  v1_message->set_nonce(std::string(nonce_bytes.begin(), nonce_bytes.end()));
  v1_message->set_payload(
      std::string(encrypted_payload.begin(), encrypted_payload.end()));

  std::string auth_message_serialized;
  auth_message.SerializeToString(&auth_message_serialized);

  return std::vector<uint8_t>(auth_message_serialized.begin(),
                              auth_message_serialized.end());
}

VerifyHandshakeMessageStatus VerifyHandshakeMessage(
    base::span<const uint8_t> auth_message_bytes,
    const std::string& auth_token,
    std::array<uint8_t, 32> secret,
    DeviceRole role) {
  std::optional<proto::V1Message> v1_message =
      ParseAuthMessage(auth_message_bytes);
  if (!v1_message) {
    return VerifyHandshakeMessageStatus::kFailedToParse;
  }

  std::optional<std::vector<uint8_t>> decrypted_bytes =
      DecryptPayload(std::vector<uint8_t>(v1_message->payload().begin(),
                                          v1_message->payload().end()),
                     secret,
                     std::vector<uint8_t>(v1_message->nonce().begin(),
                                          v1_message->nonce().end()));
  if (!decrypted_bytes) {
    QS_LOG(ERROR) << "Auth payload failed to decrypt.";
    return VerifyHandshakeMessageStatus::kFailedToDecryptAuthPayload;
  }

  std::optional<proto::V1Message::AuthenticationPayload> auth_payload =
      ParseAuthPayload(*decrypted_bytes);
  if (!auth_payload) {
    return VerifyHandshakeMessageStatus::kFailedToParseAuthPayload;
  }

  if (auth_payload->role() != static_cast<int32_t>(role)) {
    QS_LOG(ERROR) << "AuthenticationPayload role does not match expected role ("
                  << (role == DeviceRole::kSource ? "Source" : "Target")
                  << ").";
    return VerifyHandshakeMessageStatus::kUnexpectedAuthPayloadRole;
  }

  if (auth_payload->auth_string() != auth_token) {
    QS_LOG(ERROR)
        << "AuthenticationPayload auth_string does not match auth token.";
    return VerifyHandshakeMessageStatus::kUnexpectedAuthPayloadAuthToken;
  }
  return VerifyHandshakeMessageStatus::kSuccess;
}

QuickStartMetrics::HandshakeErrorCode MapHandshakeStatusToErrorCode(
    handshake::VerifyHandshakeMessageStatus handshake_status) {
  switch (handshake_status) {
    case VerifyHandshakeMessageStatus::kSuccess:
      return QuickStartMetrics::HandshakeErrorCode::kInvalidHandshakeErrorCode;
    case VerifyHandshakeMessageStatus::kFailedToParse:
      return QuickStartMetrics::HandshakeErrorCode::kFailedToParse;
    case VerifyHandshakeMessageStatus::kFailedToDecryptAuthPayload:
      return QuickStartMetrics::HandshakeErrorCode::kFailedToDecryptAuthPayload;
    case VerifyHandshakeMessageStatus::kFailedToParseAuthPayload:
      return QuickStartMetrics::HandshakeErrorCode::kFailedToParseAuthPayload;
    case VerifyHandshakeMessageStatus::kUnexpectedAuthPayloadRole:
      return QuickStartMetrics::HandshakeErrorCode::kUnexpectedAuthPayloadRole;
    case VerifyHandshakeMessageStatus::kUnexpectedAuthPayloadAuthToken:
      return QuickStartMetrics::HandshakeErrorCode::
          kUnexpectedAuthPayloadAuthToken;
  }
}

}  // namespace ash::quick_start::handshake
