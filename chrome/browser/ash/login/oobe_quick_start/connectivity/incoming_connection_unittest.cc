// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/oobe_quick_start/connectivity/incoming_connection.h"

#include "base/strings/string_number_conversions.h"
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

// 10 random bytes to use as the RandomSessionId.
constexpr std::array<uint8_t, 10> kRandomSessionId = {
    0x6b, 0xb3, 0x85, 0x27, 0xbb, 0x28, 0xb4, 0x59, 0x16, 0xca};

// Hex representation of kRandomSessionId. kRandomSessionId is converted into a
// hex string, then each character of that string is represented as a byte
// below.
constexpr std::array<uint8_t, 20> kRandomSessionIdHex = {
    0x36, 0x42, 0x42, 0x33, 0x38, 0x35, 0x32, 0x37, 0x42, 0x42,
    0x32, 0x38, 0x42, 0x34, 0x35, 0x39, 0x31, 0x36, 0x43, 0x41};

// 32 random bytes to use as the shared secret.
constexpr std::array<uint8_t, 32> kSharedSecret = {
    0x54, 0xbd, 0x40, 0xcf, 0x8a, 0x7c, 0x2f, 0x6a, 0xca, 0x15, 0x59,
    0xcf, 0xf3, 0xeb, 0x31, 0x08, 0x90, 0x73, 0xef, 0xda, 0x87, 0xd4,
    0x23, 0xc0, 0x55, 0xd5, 0x83, 0x5b, 0x04, 0x28, 0x49, 0xf2};

// Hex representation of kSharedSecret. kSharedSecret is converted into a hex
// string, then each character of that string is represented as a byte below.
constexpr std::array<uint8_t, 64> kSharedSecretHex = {
    0x35, 0x34, 0x42, 0x44, 0x34, 0x30, 0x43, 0x46, 0x38, 0x41, 0x37,
    0x43, 0x32, 0x46, 0x36, 0x41, 0x43, 0x41, 0x31, 0x35, 0x35, 0x39,
    0x43, 0x46, 0x46, 0x33, 0x45, 0x42, 0x33, 0x31, 0x30, 0x38, 0x39,
    0x30, 0x37, 0x33, 0x45, 0x46, 0x44, 0x41, 0x38, 0x37, 0x44, 0x34,
    0x32, 0x33, 0x43, 0x30, 0x35, 0x35, 0x44, 0x35, 0x38, 0x33, 0x35,
    0x42, 0x30, 0x34, 0x32, 0x38, 0x34, 0x39, 0x46, 0x32};

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
        nearby_connection, session_id, kSharedSecret);
  }

  std::unique_ptr<IncomingConnection> incoming_connection_;
  std::unique_ptr<FakeNearbyConnection> fake_nearby_connection_;
};

TEST_F(IncomingConnectionTest, TestGetQrCodeData) {
  std::vector<uint8_t> expected_data(std::begin(kBaseUrl), std::end(kBaseUrl));
  expected_data.insert(expected_data.end(), std::begin(kRandomSessionIdHex),
                       std::end(kRandomSessionIdHex));
  expected_data.insert(expected_data.end(), std::begin(kUrlKeyParam),
                       std::end(kUrlKeyParam));
  expected_data.insert(expected_data.end(), std::begin(kSharedSecretHex),
                       std::end(kSharedSecretHex));

  std::vector<uint8_t> actual_data = incoming_connection_->GetQrCodeData();

  EXPECT_EQ(expected_data, actual_data);
}

}  // namespace ash::quick_start
