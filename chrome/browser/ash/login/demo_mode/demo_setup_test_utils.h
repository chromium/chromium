// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_DEMO_MODE_DEMO_SETUP_TEST_UTILS_H_
#define CHROME_BROWSER_ASH_LOGIN_DEMO_MODE_DEMO_SETUP_TEST_UTILS_H_

#include <string>

#include "base/files/scoped_temp_dir.h"
#include "chrome/browser/ash/login/enrollment/enterprise_enrollment_helper.h"
#include "chrome/browser/ash/login/enrollment/enterprise_enrollment_helper_mock.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_config.h"
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace test {

// Result of Demo Mode setup.
// TODO(agawronska, wzang): Test more error types.
enum class DemoModeSetupResult {
  SUCCESS,
  ERROR_DEFAULT,
  ERROR_POWERWASH_REQUIRED
};

// Helper method that mocks EnterpriseEnrollmentHelper to ensure that no
// enrollment attempt was made.
void SetupMockDemoModeNoEnrollmentHelper();

// Helper method that mocks EnterpriseEnrollmentHelper for online Demo Mode
// setup. It simulates specified Demo Mode enrollment `result`.
void SetupMockDemoModeOnlineEnrollmentHelper(DemoModeSetupResult result);

// Helper method that mocks EnterpriseEnrollmentHelper for offline Demo Mode
// setup. It simulates specified Demo Mode enrollment `result`.
void SetupMockDemoModeOfflineEnrollmentHelper(DemoModeSetupResult result);

// Creates fake offline policy directory to be used in tests.
bool SetupDummyOfflinePolicyDir(const std::string& account_id,
                                base::ScopedTempDir* temp_dir);

// Set Install Attributes as part of faked enrollment so that the device is
// considered enterprise managed.
// Note: Use this for browser tests where g_install_attributes_ is already
// initialized. For unit tests, prefer |ScopedStubInstallAttributes|.
void LockDemoDeviceInstallAttributes();

}  // namespace test
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_DEMO_MODE_DEMO_SETUP_TEST_UTILS_H_
