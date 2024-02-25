// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/oobe_quick_start/connectivity/handshake_helpers.h"

#include <optional>
#include <string>
#include <vector>

#include "chrome/browser/ash/login/oobe_quick_start/connectivity/proto/aes_gcm_authentication_message.pb.h"
#include "crypto/aead.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::quick_start::handshake {

namespace {

// Arbitrary string to use as the connection's authentication token.
constexpr char kAuthToken[] = "auth_token";

// Another auth token that does not match |kAuthToken|.
constexpr char kAuthToken2[] = "auth_token_2";

// 32 random bytes to use as the shared secret.
constexpr std::array<uint8_t, 32> kSharedSecret = {
    0x54, 0xbd, 0x40, 0xcf, 0x8a, 0x7c, 0x2f, 0x6a, 0xca, 0x15, 0x59,
    0xcf, 0xf3, 0xeb, 0x31, 0x08, 0x90, 0x73, 0xef, 0xda, 0x87, 0xd4,
    0x23, 0xc0, 0x55, 0xd5, 0x83, 0x5b, 0x04, 0x28, 0x49, 0xf2};

// Another shared secret that does not match |kSharedSecret|.
constexpr std::array<uint8_t, 32> kSharedSecret2 = {
    0x00, 0xbd, 0x40, 0xcf, 0x8a, 0x7c, 0x2f, 0x6a, 0xca, 0x15, 0x59,
    0xcf, 0xf3, 0xeb, 0x31, 0x08, 0x90, 0x73, 0xef, 0xda, 0x87, 0xd4,
    0x23, 0xc0, 0x55, 0xd5, 0x83, 0x5b, 0x04, 0x28, 0x49, 0xf2};

// 12 random bytes to use as the nonce.
constexpr std::array<uint8_t, 12> kNonce = {0x60, 0x3e, 0x87, 0x69, 0xa3, 0x55,
                                            0xd3, 0x49, 0xbd, 0x0a, 0x63, 0xed};

// Some nonsense data that shouldn't parse to anything.
constexpr std::array<uint8_t, 3> kBadData = {0x01, 0x02, 0x03};

std::vector<uint8_t> BuildRawAuthMessage(
    std::optional<proto::AesGcmAuthenticationMessage::Version> version,
    std::optional<base::span<const uint8_t>> payload,
    std::optional<base::span<const uint8_t>> nonce) {
  proto::AesGcmAuthenticationMessage auth_message;

  if (version) {
    auth_message.set_version(*version);
  }
  proto::V1Message* v1 = auth_message.mutable_v1();
  if (payload) {
    crypto::Aead aead(crypto::Aead::AES_256_GCM);
    aead.Init(kSharedSecret);
    std::vector<uint8_t> enc_payload = aead.Seal(
        std::vector<uint8_t>(payload->begin(), payload->end()), kNonce,
        /*additional_data=*/base::span<uint8_t>());

    v1->set_payload(std::string(enc_payload.begin(), enc_payload.end()));
  }
  if (nonce) {
    v1->set_nonce(std::string(nonce->begin(), nonce->end()));
  }

  std::string serialized_auth_message;
  auth_message.SerializeToString(&serialized_auth_message);
  return std::vector<uint8_t>(serialized_auth_message.begin(),
                              serialized_auth_message.end());
}

std::vector<uint8_t> BuildRawAuthPayload(
    std::optional<int32_t> role,
    std::optional<std::string> auth_string) {
  proto::V1Message::AuthenticationPayload auth_payload;

  if (role) {
    auth_payload.set_role(*role);
  }
  if (auth_string) {
    auth_payload.set_auth_string(*auth_string);
  }

  std::string serialized_auth_payload;
  auth_payload.SerializeToString(&serialized_auth_payload);
  return std::vector<uint8_t>(serialized_auth_payload.begin(),
                              serialized_auth_payload.end());
}

struct VerifyHandshakeMessageTestCase {
  std::string name;
  std::vector<uint8_t> handshake_message;
  handshake::VerifyHandshakeMessageStatus expected_status;
};

const VerifyHandshakeMessageTestCase kVerifyHandshakeMessageTestCases[] = {
    {"Success",
     BuildHandshakeMessage(kAuthToken,
                           kSharedSecret,
                           kNonce,
                           DeviceRole::kSource),
     /*expected_status=*/handshake::VerifyHandshakeMessageStatus::kSuccess},
    {"TargetRole",
     BuildHandshakeMessage(kAuthToken,
                           kSharedSecret,
                           kNonce,
                           DeviceRole::kTarget),
     /*expected_status=*/
     handshake::VerifyHandshakeMessageStatus::kUnexpectedAuthPayloadRole},
    {"BadSecret",
     BuildHandshakeMessage(kAuthToken,
                           kSharedSecret2,
                           kNonce,
                           DeviceRole::kSource),
     /*expected_status=*/
     handshake::VerifyHandshakeMessageStatus::kFailedToDecryptAuthPayload},
    {"BadAuthToken",
     BuildHandshakeMessage(kAuthToken2,
                           kSharedSecret,
                           kNonce,
                           DeviceRole::kSource),
     /*expected_status=*/
     handshake::VerifyHandshakeMessageStatus::kUnexpectedAuthPayloadAuthToken},
    {"UnparsableAuthMessage",
     std::vector<uint8_t>(kBadData.begin(), kBadData.end()),
     /*expected_status=*/
     handshake::VerifyHandshakeMessageStatus::kFailedToParse},
    {"UnknownVersion",
     BuildRawAuthMessage(
         proto::AesGcmAuthenticationMessage::UNKNOWN_VERSION,
         BuildRawAuthPayload(static_cast<int32_t>(DeviceRole::kSource),
                             kAuthToken),
         kNonce),
     /*expected_status=*/
     handshake::VerifyHandshakeMessageStatus::kFailedToParse},
    {"MissingVersion",
     BuildRawAuthMessage(
         std::nullopt,
         BuildRawAuthPayload(static_cast<int32_t>(DeviceRole::kSource),
                             kAuthToken),
         kNonce),
     /*expected_status=*/
     handshake::VerifyHandshakeMessageStatus::kFailedToParse},
    {"UnparsablePayload",
     BuildRawAuthMessage(proto::AesGcmAuthenticationMessage::V1,
                         kBadData,
                         kNonce),
     /*expected_status=*/
     handshake::VerifyHandshakeMessageStatus::kFailedToParseAuthPayload},
    {"MissingPayload",
     BuildRawAuthMessage(proto::AesGcmAuthenticationMessage::V1,
                         std::nullopt,
                         kNonce),
     /*expected_status=*/
     handshake::VerifyHandshakeMessageStatus::kFailedToParse},
    {"BadNonce",
     BuildRawAuthMessage(
         proto::AesGcmAuthenticationMessage::V1,
         BuildRawAuthPayload(static_cast<int32_t>(DeviceRole::kSource),
                             kAuthToken),
         kBadData),
     /*expected_status=*/
     handshake::VerifyHandshakeMessageStatus::kFailedToParse},
    {"MissingNonce",
     BuildRawAuthMessage(
         proto::AesGcmAuthenticationMessage::V1,
         BuildRawAuthPayload(static_cast<int32_t>(DeviceRole::kSource),
                             kAuthToken),
         std::nullopt),
     /*expected_status=*/
     handshake::VerifyHandshakeMessageStatus::kFailedToParse},
    {"BadRole",
     BuildRawAuthMessage(proto::AesGcmAuthenticationMessage::V1,
                         BuildRawAuthPayload(3, kAuthToken),
                         kNonce),
     /*expected_status=*/
     handshake::VerifyHandshakeMessageStatus::kUnexpectedAuthPayloadRole},
    {"MissingRole",
     BuildRawAuthMessage(proto::AesGcmAuthenticationMessage::V1,
                         BuildRawAuthPayload(std::nullopt, kAuthToken),
                         kNonce),
     /*expected_status=*/
     handshake::VerifyHandshakeMessageStatus::kFailedToParseAuthPayload},
    {"MissingAuthString",
     BuildRawAuthMessage(
         proto::AesGcmAuthenticationMessage::V1,
         BuildRawAuthPayload(static_cast<int32_t>(DeviceRole::kSource),
                             std::nullopt),
         kNonce),
     /*expected_status=*/
     handshake::VerifyHandshakeMessageStatus::kFailedToParseAuthPayload},
};

}  // namespace

TEST(HandshakeHelpersTest, VerifyHandshakeMessage) {
  for (const VerifyHandshakeMessageTestCase& test_case :
       kVerifyHandshakeMessageTestCases) {
    handshake::VerifyHandshakeMessageStatus status = VerifyHandshakeMessage(
        test_case.handshake_message, kAuthToken, kSharedSecret);
    EXPECT_EQ(test_case.expected_status, status)
        << "Testcase " << test_case.name << " failed";
    EXPECT_EQ(MapHandshakeStatusToErrorCode(test_case.expected_status),
              MapHandshakeStatusToErrorCode(status));
  }
}

}  // namespace ash::quick_start::handshake
