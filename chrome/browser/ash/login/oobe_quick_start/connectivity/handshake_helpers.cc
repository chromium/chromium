// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/oobe_quick_start/connectivity/handshake_helpers.h"

#include "base/containers/span.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/proto/aes_gcm_authentication_message.pb.h"
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

}  // namespace

std::vector<uint8_t> BuildHandshakeMessage(
    const std::string& auth_token,
    std::array<uint8_t, 32> secret,
    absl::optional<std::array<uint8_t, 12>> nonce,
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

}  // namespace ash::quick_start::handshake
