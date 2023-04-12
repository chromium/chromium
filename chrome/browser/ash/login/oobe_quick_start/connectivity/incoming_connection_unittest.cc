// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/oobe_quick_start/connectivity/incoming_connection.h"

#include <memory>

#include "chrome/browser/ash/login/oobe_quick_start/connectivity/random_session_id.h"
#include "chrome/browser/nearby_sharing/fake_nearby_connection.h"
#include "chrome/browser/nearby_sharing/public/cpp/nearby_connection.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::quick_start {

namespace {

// Base qr code url ("https://signin.google/qs/") represented in a 25 byte
// array.
constexpr std::array<uint8_t, 25> kBaseUrl = {
    0x68, 0x74, 0x74, 0x70, 0x73, 0x3a, 0x2f, 0x2f, 0x73,
    0x69, 0x67, 0x6e, 0x69, 0x6e, 0x2e, 0x67, 0x6f, 0x6f,
    0x67, 0x6c, 0x65, 0x2f, 0x71, 0x73, 0x2f};

// Qr code key param ("?key=") represented in a 5 byte array.
constexpr std::array<uint8_t, 5> kUrlKeyParam = {0x3f, 0x6b, 0x65, 0x79, 0x3d};

// 6 random bytes to use as the RandomSessionId.
constexpr std::array<uint8_t, 6> kRandomSessionId = {0x6b, 0xb3, 0x85,
                                                     0x27, 0xbb, 0x28};

// Base64 representation of kRandomSessionId.
constexpr char kRandomSessionIdBase64[] = "a7OFJ7so";

// 32 random bytes to use as the shared secret.
constexpr std::array<uint8_t, 32> kSharedSecret = {
    0x54, 0xbd, 0x40, 0xcf, 0x8a, 0x7c, 0x2f, 0x6a, 0xca, 0x15, 0x59,
    0xcf, 0xf3, 0xeb, 0x31, 0x08, 0x90, 0x73, 0xef, 0xda, 0x87, 0xd4,
    0x23, 0xc0, 0x55, 0xd5, 0x83, 0x5b, 0x04, 0x28, 0x49, 0xf2};

// Base64 representation of kSharedSecret.
constexpr char kSharedSecretBase64[] =
    "VL1Az4p8L2rKFVnP8-sxCJBz79qH1CPAVdWDWwQoSfI";

// Arbitrary string to use as the connection's authentication token.
constexpr char kAuthenticationToken[] = "auth_token";

// Pin corresponding to |kAuthenticationToken|.
constexpr char kAuthenticationTokenPin[] = "6229";

}  // namespace

class IncomingConnectionTest : public testing::Test {
 public:
  IncomingConnectionTest(const IncomingConnectionTest&) = delete;
  IncomingConnectionTest& operator=(const IncomingConnectionTest&) = delete;

 protected:
  IncomingConnectionTest() = default;

  void SetUp() override {
    RandomSessionId session_id(kRandomSessionId);
    fake_nearby_connection_ = std::make_unique<FakeNearbyConnection>();
    NearbyConnection* nearby_connection = fake_nearby_connection_.get();
    incoming_connection_ = std::make_unique<IncomingConnection>(
        nearby_connection, session_id, kAuthenticationToken, kSharedSecret);
  }

  std::unique_ptr<IncomingConnection> incoming_connection_;
  std::unique_ptr<FakeNearbyConnection> fake_nearby_connection_;
};

TEST_F(IncomingConnectionTest, TestGetQrCodeData) {
  std::string session_id(kRandomSessionIdBase64);
  std::string shared_secret(kSharedSecretBase64);

  std::vector<uint8_t> expected_data(std::begin(kBaseUrl), std::end(kBaseUrl));
  expected_data.insert(expected_data.end(), session_id.begin(),
                       session_id.end());
  expected_data.insert(expected_data.end(), std::begin(kUrlKeyParam),
                       std::end(kUrlKeyParam));
  expected_data.insert(expected_data.end(), shared_secret.begin(),
                       shared_secret.end());

  std::vector<uint8_t> actual_data = incoming_connection_->GetQrCodeData();

  EXPECT_EQ(expected_data, actual_data);
}

TEST_F(IncomingConnectionTest, GetConnectionVerificationPin) {
  EXPECT_EQ(kAuthenticationTokenPin,
            incoming_connection_->GetConnectionVerificationPin());
}

}  // namespace ash::quick_start
