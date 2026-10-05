// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_reauth/chromeos/device_auth_availability_checker_chromeos.h"

#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/device_reauth/chromeos/authenticator_chromeos.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using ::testing::Return;

class MockSystemAuthenticator : public AuthenticatorChromeOSInterface {
 public:
  MOCK_METHOD(void,
              AuthenticateUser,
              (const std::u16string& message,
               device_reauth::DeviceAuthSource source,
               base::OnceCallback<void(bool)> callback),
              (override));
  MOCK_METHOD(BiometricsStatusChromeOS,
              CheckIfBiometricsAvailable,
              (),
              (override));
  MOCK_METHOD(void,
              CheckIfPinIsAvailable,
              (base::OnceCallback<void(bool)>),
              (override));
};

class DeviceAuthAvailabilityCheckerChromeOSTest : public testing::Test {
 public:
  void SetUp() override {
    TestingPrefServiceSimple* prefs =
        TestingBrowserProcess::GetGlobal()->GetTestingLocalState();

    if (!prefs->FindPreference(
            password_manager::prefs::kHadBiometricsAvailable)) {
      prefs->registry()->RegisterBooleanPref(
          password_manager::prefs::kHadBiometricsAvailable, false);
    }

    if (!prefs->FindPreference(
            password_manager::prefs::kPinAuthenticationAvailableOnChromeOS)) {
      prefs->registry()->RegisterBooleanPref(
          password_manager::prefs::kPinAuthenticationAvailableOnChromeOS,
          false);
    }

    std::unique_ptr<MockSystemAuthenticator> system_authenticator =
        std::make_unique<MockSystemAuthenticator>();
    system_authenticator_ = system_authenticator.get();
    checker_ = std::make_unique<DeviceAuthAvailabilityCheckerChromeOS>(
        std::move(system_authenticator));
  }

  DeviceAuthAvailabilityCheckerChromeOS* checker() { return checker_.get(); }

  MockSystemAuthenticator& system_authenticator() {
    return *system_authenticator_;
  }

  PrefService* local_state() {
    return TestingBrowserProcess::GetGlobal()->local_state();
  }

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

 private:
  std::unique_ptr<DeviceAuthAvailabilityCheckerChromeOS> checker_;
  raw_ptr<MockSystemAuthenticator> system_authenticator_ = nullptr;
  base::HistogramTester histogram_tester_;
};

TEST_F(DeviceAuthAvailabilityCheckerChromeOSTest, BiometricsAvailable) {
  EXPECT_CALL(system_authenticator(), CheckIfBiometricsAvailable)
      .WillOnce(Return(BiometricsStatusChromeOS::kAvailable));

  EXPECT_TRUE(checker()->CanAuthenticateWithBiometrics());
  EXPECT_TRUE(local_state()->GetBoolean(
      password_manager::prefs::kHadBiometricsAvailable));
  histogram_tester().ExpectUniqueSample(
      "PasswordManager.BiometricAvailabilityChromeOS",
      BiometricsStatusChromeOS::kAvailable, 1);
}

TEST_F(DeviceAuthAvailabilityCheckerChromeOSTest, BiometricsUnavailable) {
  EXPECT_CALL(system_authenticator(), CheckIfBiometricsAvailable)
      .WillOnce(Return(BiometricsStatusChromeOS::kUnavailable));

  EXPECT_FALSE(checker()->CanAuthenticateWithBiometrics());
  EXPECT_FALSE(local_state()->GetBoolean(
      password_manager::prefs::kHadBiometricsAvailable));
  histogram_tester().ExpectUniqueSample(
      "PasswordManager.BiometricAvailabilityChromeOS",
      BiometricsStatusChromeOS::kUnavailable, 1);
}

TEST_F(DeviceAuthAvailabilityCheckerChromeOSTest,
       BiometricsNotConfiguredForUser) {
  EXPECT_CALL(system_authenticator(), CheckIfBiometricsAvailable)
      .WillOnce(Return(BiometricsStatusChromeOS::kNotConfiguredForUser));

  EXPECT_FALSE(checker()->CanAuthenticateWithBiometrics());
  EXPECT_FALSE(local_state()->GetBoolean(
      password_manager::prefs::kHadBiometricsAvailable));
  histogram_tester().ExpectUniqueSample(
      "PasswordManager.BiometricAvailabilityChromeOS",
      BiometricsStatusChromeOS::kNotConfiguredForUser, 1);
}

TEST_F(DeviceAuthAvailabilityCheckerChromeOSTest,
       BiometricsUnavailableScreenLockAvailable) {
  EXPECT_CALL(system_authenticator(), CheckIfBiometricsAvailable)
      .WillOnce(Return(BiometricsStatusChromeOS::kUnavailable));
  local_state()->SetBoolean(
      password_manager::prefs::kPinAuthenticationAvailableOnChromeOS, true);

  EXPECT_TRUE(checker()->CanAuthenticateWithBiometricOrScreenLock());
}

TEST_F(DeviceAuthAvailabilityCheckerChromeOSTest,
       BiometricsAvailableScreenLockUnavailable) {
  EXPECT_CALL(system_authenticator(), CheckIfBiometricsAvailable)
      .WillOnce(Return(BiometricsStatusChromeOS::kAvailable));
  local_state()->SetBoolean(
      password_manager::prefs::kPinAuthenticationAvailableOnChromeOS, false);

  EXPECT_TRUE(checker()->CanAuthenticateWithBiometricOrScreenLock());
}

TEST_F(DeviceAuthAvailabilityCheckerChromeOSTest, NeitherAvailable) {
  EXPECT_CALL(system_authenticator(), CheckIfBiometricsAvailable)
      .WillOnce(Return(BiometricsStatusChromeOS::kUnavailable));
  local_state()->SetBoolean(
      password_manager::prefs::kPinAuthenticationAvailableOnChromeOS, false);

  EXPECT_FALSE(checker()->CanAuthenticateWithBiometricOrScreenLock());
}

}  // namespace
