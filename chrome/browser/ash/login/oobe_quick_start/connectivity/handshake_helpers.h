// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_CONNECTIVITY_HANDSHAKE_HELPERS_H_
#define CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_CONNECTIVITY_HANDSHAKE_HELPERS_H_

#include <array>
#include <optional>
#include <vector>

#include "base/containers/span.h"
#include "chromeos/ash/components/quick_start/quick_start_metrics.h"

namespace ash::quick_start::handshake {

// The role for the device producing a handshake message. Values come from
// this enum:
// http://google3/java/com/google/android/gms/smartdevice/d2d/proto/aes_gcm_authentication_message.proto;l=26;rcl=489093041
enum class DeviceRole {
  kTarget = 1,
  kSource = 2,
};

// Status when verifying handshake message.
enum class VerifyHandshakeMessageStatus {
  kSuccess,
  kFailedToParse,
  kFailedToDecryptAuthPayload,
  kFailedToParseAuthPayload,
  kUnexpectedAuthPayloadRole,
  kUnexpectedAuthPayloadAuthToken,
};

// Build and serialize an AesGcmAuthenticationMessage proto. If |nonce| and
// |role| are not provided, then a new random nonce and the target device role
// will be used.
std::vector<uint8_t> BuildHandshakeMessage(
    const std::string& auth_token,
    std::array<uint8_t, 32> secret,
    std::optional<std::array<uint8_t, 12>> nonce = std::nullopt,
    DeviceRole role = DeviceRole::kTarget);

// Decode an AesGcmAuthenticationMessage proto, attempt to decrypt the auth
// payload using the secret, and verify that the payload contains the correct
// role and auth token. If the return value is
// VerifyHandshakeMessageStatus::kSuccess, then the remote device is
// authenticated.
VerifyHandshakeMessageStatus VerifyHandshakeMessage(
    base::span<const uint8_t> auth_message_bytes,
    const std::string& auth_token,
    std::array<uint8_t, 32> secret,
    DeviceRole role = DeviceRole::kSource);

QuickStartMetrics::HandshakeErrorCode MapHandshakeStatusToErrorCode(
    VerifyHandshakeMessageStatus handshake_status);

}  // namespace ash::quick_start::handshake

#endif  // CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_CONNECTIVITY_HANDSHAKE_HELPERS_H_
