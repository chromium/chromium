// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_DEMO_MODE_DEMO_SETUP_TEST_UTILS_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_DEMO_MODE_DEMO_SETUP_TEST_UTILS_H_

#include <string>

#include "base/files/scoped_temp_dir.h"
#include "chrome/browser/chromeos/login/enrollment/enterprise_enrollment_helper.h"
#include "chrome/browser/chromeos/login/enrollment/enterprise_enrollment_helper_mock.h"
#include "chrome/browser/chromeos/policy/enrollment_config.h"
#include "chrome/browser/chromeos/policy/enrollment_status_chromeos.h"
#include "chromeos/settings/install_attributes.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace test {

// Result of Demo Mode setup.
enum class DemoModeSetupResult { SUCCESS, ERROR };

// Helper method that mocks EnterpriseEnrollmentHelper for online Demo Mode
// setup. It simulates specified Demo Mode enrollment |result|.
template <DemoModeSetupResult result>
EnterpriseEnrollmentHelper* MockDemoModeOnlineEnrollmentHelperCreator(
    EnterpriseEnrollmentHelper::EnrollmentStatusConsumer* status_consumer,
    const policy::EnrollmentConfig& enrollment_config,
    const std::string& enrolling_user_domain) {
  EnterpriseEnrollmentHelperMock* mock =
      new EnterpriseEnrollmentHelperMock(status_consumer);

  EXPECT_EQ(enrollment_config.mode, policy::EnrollmentConfig::MODE_ATTESTATION);
  EXPECT_CALL(*mock, EnrollUsingAttestation())
      .WillRepeatedly(testing::Invoke([mock]() {
        if (result == DemoModeSetupResult::SUCCESS) {
          mock->status_consumer()->OnDeviceEnrolled("");
        } else {
          // TODO(agawronska): Test different error types.
          mock->status_consumer()->OnEnrollmentError(
              policy::EnrollmentStatus::ForRegistrationError(
                  policy::DeviceManagementStatus::
                      DM_STATUS_TEMPORARY_UNAVAILABLE));
        }
      }));
  return mock;
}

// Helper method that mocks EnterpriseEnrollmentHelper for offline Demo Mode
// setup. It simulates specified Demo Mode enrollment |result|.
template <DemoModeSetupResult result>
EnterpriseEnrollmentHelper* MockDemoModeOfflineEnrollmentHelperCreator(
    EnterpriseEnrollmentHelper::EnrollmentStatusConsumer* status_consumer,
    const policy::EnrollmentConfig& enrollment_config,
    const std::string& enrolling_user_domain) {
  EnterpriseEnrollmentHelperMock* mock =
      new EnterpriseEnrollmentHelperMock(status_consumer);

  EXPECT_EQ(enrollment_config.mode,
            policy::EnrollmentConfig::MODE_OFFLINE_DEMO);
  EXPECT_CALL(*mock, EnrollForOfflineDemo())
      .WillRepeatedly(testing::Invoke([mock]() {
        if (result == DemoModeSetupResult::SUCCESS) {
          mock->status_consumer()->OnDeviceEnrolled("");
        } else {
          // TODO(agawronska): Test different error types.
          mock->status_consumer()->OnEnrollmentError(
              policy::EnrollmentStatus::ForLockError(
                  chromeos::InstallAttributes::LOCK_READBACK_ERROR));
        }
      }));
  return mock;
}

// Creates fake offline policy directory to be used in tests.
bool SetupDummyOfflinePolicyDir(const std::string& account_id,
                                base::ScopedTempDir* temp_dir);

}  // namespace test

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_DEMO_MODE_DEMO_SETUP_TEST_UTILS_H_
