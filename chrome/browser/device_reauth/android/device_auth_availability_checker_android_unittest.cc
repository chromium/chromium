// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_reauth/android/device_auth_availability_checker_android.h"

#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/device_reauth/android/device_authenticator_bridge.h"
#include "components/device_reauth/device_auth_availability_checker.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using device_reauth::BiometricsAvailability;
using device_reauth::BiometricStatus;
using testing::Return;

class MockBridge : public DeviceAuthAvailabilityCheckerAndroid::Bridge {
 public:
  MockBridge() = default;
  ~MockBridge() override = default;

  MOCK_METHOD(BiometricsAvailability,
              CanAuthenticateWithBiometric,
              (),
              (override));
  MOCK_METHOD(bool, CanAuthenticateWithBiometricOrScreenLock, (), (override));
};

class DeviceAuthAvailabilityCheckerAndroidTest : public testing::Test {
 public:
  void SetUp() override {
    auto bridge = std::make_unique<MockBridge>();
    bridge_ = bridge.get();
    checker_ = std::make_unique<DeviceAuthAvailabilityCheckerAndroid>(
        std::move(bridge));
  }

  DeviceAuthAvailabilityCheckerAndroid* checker() { return checker_.get(); }
  MockBridge& bridge() { return *bridge_; }

 private:
  std::unique_ptr<DeviceAuthAvailabilityCheckerAndroid> checker_;
  raw_ptr<MockBridge> bridge_ = nullptr;
};

TEST_F(DeviceAuthAvailabilityCheckerAndroidTest,
       CanAuthenticateWithBiometricsAvailable) {
  EXPECT_CALL(bridge(), CanAuthenticateWithBiometric)
      .WillOnce(Return(BiometricsAvailability::kAvailable));

  EXPECT_TRUE(checker()->CanAuthenticateWithBiometrics());
}

TEST_F(DeviceAuthAvailabilityCheckerAndroidTest,
       CanAuthenticateWithBiometricsUnavailable) {
  EXPECT_CALL(bridge(), CanAuthenticateWithBiometric)
      .WillOnce(Return(BiometricsAvailability::kNotEnrolled));

  EXPECT_FALSE(checker()->CanAuthenticateWithBiometrics());
}

TEST_F(DeviceAuthAvailabilityCheckerAndroidTest,
       CanAuthenticateWithBiometricOrScreenLockAvailable) {
  EXPECT_CALL(bridge(), CanAuthenticateWithBiometricOrScreenLock)
      .WillOnce(Return(true));

  EXPECT_TRUE(checker()->CanAuthenticateWithBiometricOrScreenLock());
}

TEST_F(DeviceAuthAvailabilityCheckerAndroidTest,
       CanAuthenticateWithBiometricOrScreenLockUnavailable) {
  EXPECT_CALL(bridge(), CanAuthenticateWithBiometricOrScreenLock)
      .WillOnce(Return(false));

  EXPECT_FALSE(checker()->CanAuthenticateWithBiometricOrScreenLock());
}

TEST_F(DeviceAuthAvailabilityCheckerAndroidTest,
       GetBiometricAvailabilityStatusBiometricsAvailable) {
  EXPECT_CALL(bridge(), CanAuthenticateWithBiometric)
      .WillOnce(Return(BiometricsAvailability::kAvailable));

  EXPECT_EQ(checker()->GetBiometricAvailabilityStatus(),
            BiometricStatus::kBiometricsAvailable);
}

TEST_F(DeviceAuthAvailabilityCheckerAndroidTest,
       GetBiometricAvailabilityStatusOnlyLskfAvailable) {
  EXPECT_CALL(bridge(), CanAuthenticateWithBiometric)
      .WillOnce(Return(BiometricsAvailability::kNotEnrolled));
  EXPECT_CALL(bridge(), CanAuthenticateWithBiometricOrScreenLock)
      .WillOnce(Return(true));

  EXPECT_EQ(checker()->GetBiometricAvailabilityStatus(),
            BiometricStatus::kOnlyLskfAvailable);
}

TEST_F(DeviceAuthAvailabilityCheckerAndroidTest,
       GetBiometricAvailabilityStatusUnavailable) {
  EXPECT_CALL(bridge(), CanAuthenticateWithBiometric)
      .WillOnce(Return(BiometricsAvailability::kNoHardware));
  EXPECT_CALL(bridge(), CanAuthenticateWithBiometricOrScreenLock)
      .WillOnce(Return(false));

  EXPECT_EQ(checker()->GetBiometricAvailabilityStatus(),
            BiometricStatus::kUnavailable);
}

}  // namespace
