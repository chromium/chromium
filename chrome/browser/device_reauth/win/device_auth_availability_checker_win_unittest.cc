// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_reauth/win/device_auth_availability_checker_win.h"

#include <memory>
#include <string>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/device_reauth/win/authenticator_win.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using base::test::RunOnceCallback;
using testing::Return;

class MockSystemAuthenticator : public AuthenticatorWinInterface {
 public:
  MOCK_METHOD(void,
              AuthenticateUser,
              (const std::u16string& message,
               base::OnceCallback<void(bool)> callback),
              (override));
  MOCK_METHOD(void,
              CheckIfBiometricsAvailable,
              (AvailabilityCallback callback),
              (override));
  MOCK_METHOD(bool, CanAuthenticateWithScreenLock, (), (override));
};

class DeviceAuthAvailabilityCheckerWinTest : public testing::Test {
 public:
  void SetUp() override {
    std::unique_ptr<MockSystemAuthenticator> system_authenticator =
        std::make_unique<MockSystemAuthenticator>();
    system_authenticator_ = system_authenticator.get();
    checker_ = std::make_unique<DeviceAuthAvailabilityCheckerWin>(
        std::move(system_authenticator));
  }

  DeviceAuthAvailabilityCheckerWin* checker() { return checker_.get(); }

  MockSystemAuthenticator& system_authenticator() {
    return *system_authenticator_;
  }

  PrefService* local_state() {
    return TestingBrowserProcess::GetGlobal()->local_state();
  }

 private:
  std::unique_ptr<DeviceAuthAvailabilityCheckerWin> checker_;
  raw_ptr<MockSystemAuthenticator> system_authenticator_ = nullptr;
};

TEST_F(DeviceAuthAvailabilityCheckerWinTest, BiometricsAvailable) {
  local_state()->SetBoolean(password_manager::prefs::kIsBiometricAvailable,
                            true);
  EXPECT_TRUE(checker()->CanAuthenticateWithBiometrics());
  EXPECT_TRUE(checker()->CanAuthenticateWithBiometricOrScreenLock());
}

TEST_F(DeviceAuthAvailabilityCheckerWinTest,
       BiometricsUnavailableScreenLockAvailable) {
  local_state()->SetBoolean(password_manager::prefs::kIsBiometricAvailable,
                            false);
  EXPECT_CALL(system_authenticator(), CanAuthenticateWithScreenLock)
      .WillOnce(Return(true));
  EXPECT_FALSE(checker()->CanAuthenticateWithBiometrics());
  EXPECT_TRUE(checker()->CanAuthenticateWithBiometricOrScreenLock());
}

TEST_F(DeviceAuthAvailabilityCheckerWinTest, NeitherAvailable) {
  local_state()->SetBoolean(password_manager::prefs::kIsBiometricAvailable,
                            false);
  EXPECT_CALL(system_authenticator(), CanAuthenticateWithScreenLock)
      .WillOnce(Return(false));
  EXPECT_FALSE(checker()->CanAuthenticateWithBiometrics());
  EXPECT_FALSE(checker()->CanAuthenticateWithBiometricOrScreenLock());
}

struct TestCase {
  const char* description;
  BiometricAuthenticationStatusWin availability;
  bool expected_result;
  int expected_bucket;
};

class DeviceAuthAvailabilityCheckerWinTestAvailability
    : public DeviceAuthAvailabilityCheckerWinTest,
      public testing::WithParamInterface<TestCase> {};

TEST_P(DeviceAuthAvailabilityCheckerWinTestAvailability, AvailabilityCheck) {
  TestCase test_case = GetParam();
  SCOPED_TRACE(test_case.description);
  base::HistogramTester histogram_tester;
  auto authenticator = std::make_unique<MockSystemAuthenticator>();
  EXPECT_CALL(*authenticator, CheckIfBiometricsAvailable)
      .WillOnce(testing::WithArg<0>([&test_case](auto callback) {
        std::move(callback).Run(test_case.availability);
      }));

  DeviceAuthAvailabilityCheckerWin::CacheIfBiometricsAvailable(
      std::move(authenticator));

  EXPECT_EQ(test_case.expected_result,
            local_state()->GetBoolean(
                password_manager::prefs::kIsBiometricAvailable));
  EXPECT_EQ(test_case.expected_result,
            local_state()->GetBoolean(
                password_manager::prefs::kHadBiometricsAvailable));
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.BiometricAvailabilityWin", test_case.expected_bucket, 1);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    DeviceAuthAvailabilityCheckerWinTestAvailability,
    ::testing::Values(
        TestCase{
            .description = "kUnknown",
            .availability = BiometricAuthenticationStatusWin::kUnknown,
            .expected_result = false,
            .expected_bucket = 0,
        },
        TestCase{
            .description = "kAvailable",
            .availability = BiometricAuthenticationStatusWin::kAvailable,
            .expected_result = true,
            .expected_bucket = 1,
        },
        TestCase{
            .description = "kDeviceBusy",
            .availability = BiometricAuthenticationStatusWin::kDeviceBusy,
            .expected_result = false,
            .expected_bucket = 2,
        },
        TestCase{
            .description = "kDisabledByPolicy",
            .availability = BiometricAuthenticationStatusWin::kDisabledByPolicy,
            .expected_result = false,
            .expected_bucket = 3,
        },
        TestCase{
            .description = "kDeviceNotPresent",
            .availability = BiometricAuthenticationStatusWin::kDeviceNotPresent,
            .expected_result = false,
            .expected_bucket = 4,
        },
        TestCase{
            .description = "kNotConfiguredForUser",
            .availability =
                BiometricAuthenticationStatusWin::kNotConfiguredForUser,
            .expected_result = false,
            .expected_bucket = 5,
        }));

}  // namespace
