// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_reauth/mac/device_auth_availability_checker_mac.h"

#include <memory>
#include <tuple>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/device_reauth/mac/authenticator_mac.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class MockSystemAuthenticator : public AuthenticatorMacInterface {
 public:
  MOCK_METHOD(bool, CheckIfBiometricsAvailable, (), (override));
  MOCK_METHOD(bool, CheckIfBiometricsOrScreenLockAvailable, (), (override));
  MOCK_METHOD(bool,
              AuthenticateUserWithNonBiometrics,
              (const std::u16string&),
              (override));
};

class DeviceAuthAvailabilityCheckerMacTest
    : public ::testing::TestWithParam<std::tuple<bool, bool>> {
 public:
  DeviceAuthAvailabilityCheckerMacTest() {
    std::unique_ptr<MockSystemAuthenticator> system_authenticator =
        std::make_unique<MockSystemAuthenticator>();
    system_authenticator_ = system_authenticator.get();
    checker_ = std::make_unique<DeviceAuthAvailabilityCheckerMac>(
        std::move(system_authenticator));
    ON_CALL(*system_authenticator_, CheckIfBiometricsAvailable)
        .WillByDefault(testing::Return(is_biometric_available()));
    ON_CALL(*system_authenticator_, CheckIfBiometricsOrScreenLockAvailable)
        .WillByDefault(testing::Return(is_biometric_available() ||
                                       is_screen_lock_available()));
  }

  bool is_biometric_available() const { return std::get<0>(GetParam()); }
  bool is_screen_lock_available() const { return std::get<1>(GetParam()); }

  DeviceAuthAvailabilityCheckerMac* checker() { return checker_.get(); }

  MockSystemAuthenticator& system_authenticator() {
    return *system_authenticator_;
  }

  PrefService* local_state() {
    return TestingBrowserProcess::GetGlobal()->local_state();
  }

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

 private:
  std::unique_ptr<DeviceAuthAvailabilityCheckerMac> checker_;
  raw_ptr<MockSystemAuthenticator> system_authenticator_ = nullptr;
  base::HistogramTester histogram_tester_;
};

TEST_P(DeviceAuthAvailabilityCheckerMacTest,
       BiometricAuthenticationAvailability) {
  EXPECT_CALL(system_authenticator(), CheckIfBiometricsAvailable);
  EXPECT_EQ(checker()->CanAuthenticateWithBiometrics(),
            is_biometric_available());
  EXPECT_EQ(is_biometric_available(),
            local_state()->GetBoolean(
                password_manager::prefs::kHadBiometricsAvailable));
  histogram_tester().ExpectUniqueSample("PasswordManager.CanUseBiometricsMac",
                                        is_biometric_available(), 1);
}

TEST_P(DeviceAuthAvailabilityCheckerMacTest,
       BiometricAndScreenLockAuthenticationAvailability) {
  if (is_biometric_available()) {
    EXPECT_CALL(system_authenticator(), CheckIfBiometricsAvailable);
  } else {
    EXPECT_CALL(system_authenticator(), CheckIfBiometricsOrScreenLockAvailable);
  }

  EXPECT_EQ(checker()->CanAuthenticateWithBiometricOrScreenLock(),
            is_biometric_available() || is_screen_lock_available());
  EXPECT_EQ(is_biometric_available(),
            local_state()->GetBoolean(
                password_manager::prefs::kHadBiometricsAvailable));
}

INSTANTIATE_TEST_SUITE_P(,
                         DeviceAuthAvailabilityCheckerMacTest,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Bool()));

}  // namespace
