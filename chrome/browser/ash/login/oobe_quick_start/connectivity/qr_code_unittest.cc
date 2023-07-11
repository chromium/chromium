// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/oobe_quick_start/connectivity/qr_code.h"

#include <string>
#include <vector>

#include "chrome/browser/ash/login/oobe_quick_start/connectivity/random_session_id.h"
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

// Qr code device type param ("&t=7") represented in a 4 byte array.
constexpr std::array<uint8_t, 4> kUrlDeviceTypeParam = {0x26, 0x74, 0x3d, 0x37};

// 32 random bytes to use as the shared secret when generating QR Code.
constexpr std::array<uint8_t, 32> kSharedSecret = {
    0x54, 0xbd, 0x40, 0xcf, 0x8a, 0x7c, 0x2f, 0x6a, 0xca, 0x15, 0x59,
    0xcf, 0xf3, 0xeb, 0x31, 0x08, 0x90, 0x73, 0xef, 0xda, 0x87, 0xd4,
    0x23, 0xc0, 0x55, 0xd5, 0x83, 0x5b, 0x04, 0x28, 0x49, 0xf2};

// Base64 representation of kSharedSecret.
constexpr char kSharedSecretBase64[] =
    "VL1Az4p8L2rKFVnP8%2BsxCJBz79qH1CPAVdWDWwQoSfI%3D";

}  // namespace

class QRCodeTest : public testing::Test {
 public:
  QRCodeTest() = default;
  QRCodeTest(const QRCodeTest&) = delete;
  QRCodeTest& operator=(const QRCodeTest&) = delete;

  void SetUp() override {
    random_session_id_ = RandomSessionId();
    qr_code_ = std::make_unique<QRCode>(random_session_id_, kSharedSecret);
  }

 protected:
  std::vector<uint8_t> GetQRCodeData() { return qr_code_->GetQRCodeData(); }

  RandomSessionId random_session_id_;
  std::unique_ptr<QRCode> qr_code_;
};

TEST_F(QRCodeTest, GetQRCodeData) {
  std::string random_session_id = random_session_id_.ToString();
  std::string encoded_shared_secret(kSharedSecretBase64);

  std::vector<uint8_t> expected_data(std::begin(kBaseUrl), std::end(kBaseUrl));
  expected_data.insert(expected_data.end(), random_session_id.begin(),
                       random_session_id.end());
  expected_data.insert(expected_data.end(), std::begin(kUrlKeyParam),
                       std::end(kUrlKeyParam));
  expected_data.insert(expected_data.end(), encoded_shared_secret.begin(),
                       encoded_shared_secret.end());
  expected_data.insert(expected_data.end(), std::begin(kUrlDeviceTypeParam),
                       std::end(kUrlDeviceTypeParam));

  std::vector<uint8_t> actual_data = GetQRCodeData();
  EXPECT_EQ(expected_data, actual_data);
}

}  // namespace ash::quick_start
