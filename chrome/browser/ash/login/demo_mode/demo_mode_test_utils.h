// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_DEMO_MODE_DEMO_MODE_TEST_UTILS_H_
#define CHROME_BROWSER_ASH_LOGIN_DEMO_MODE_DEMO_MODE_TEST_UTILS_H_

#include <string>

#include "base/files/scoped_temp_dir.h"

namespace enterprise_management {
class DemoModeDimensions;
}  // namespace enterprise_management

namespace ash {
class MockEnrollmentLauncher;

namespace test {

// Result of Demo Mode setup.
// TODO(agawronska, wzang): Test more error types.
enum class DemoModeSetupResult {
  SUCCESS,
  ERROR_DEFAULT,
  ERROR_POWERWASH_REQUIRED
};

// Helper method that sets expectations on enrollment launcher to ensure that no
// enrollment attempt was made.
void SetupDemoModeNoEnrollment(MockEnrollmentLauncher* mock);

// Helper method that sets expectations on enrollment launcher for online Demo
// Mode setup. It simulates specified Demo Mode enrollment `result`.
void SetupDemoModeOnlineEnrollment(
    MockEnrollmentLauncher* mock_enrollment_process_launcher,
    DemoModeSetupResult result);

// Creates fake offline policy directory to be used in tests.
bool SetupDummyOfflinePolicyDir(const std::string& account_id,
                                base::ScopedTempDir* temp_dir);

// Set Install Attributes as part of faked enrollment so that the device is
// considered enterprise managed.
// Note: Use this for browser tests where g_install_attributes_ is already
// initialized. For unit tests, prefer |ScopedStubInstallAttributes|.
void LockDemoDeviceInstallAttributes();

void AssertDemoDimensionsEqual(
    const enterprise_management::DemoModeDimensions& actual,
    const enterprise_management::DemoModeDimensions& expected);

}  // namespace test
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_DEMO_MODE_DEMO_MODE_TEST_UTILS_H_
