// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/reset_screen.h"

#include "ash/constants/ash_switches.h"
#include "base/test/scoped_command_line.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/policy/enrollment/auto_enrollment_type_checker.h"
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

const char kEnrollmentDomain[] = "example.com";
const char kEnrollmentId[] = "fake-id";

void ExpectPowerwashAllowed(bool is_reset_allowed) {
  base::test::TestFuture<bool, std::optional<tpm_firmware_update::Mode>> future;
  ResetScreen::CheckIfPowerwashAllowed(future.GetCallback());
  EXPECT_EQ(is_reset_allowed, future.Get<0>());
}

}  // namespace

class ResetScreenTest : public testing::Test {
 public:
  ResetScreenTest();

  ResetScreenTest(const ResetScreenTest&) = delete;
  ResetScreenTest& operator=(const ResetScreenTest&) = delete;

  ~ResetScreenTest() override = default;

  // Configure install attributes.
  void SetUnowned();
  void SetEnterpriseOwned();
  void SetConsumerOwned();

  // Configure policy
  void SetPowerwashAllowedByPolicy(bool allowed);

  // Configure VPD values
  void SetFreOn();
  void SetFreOff();

 private:
  ScopedCrosSettingsTestHelper cros_settings_test_helper_;
  system::ScopedFakeStatisticsProvider fake_statistics_provider_;
  content::BrowserTaskEnvironment browser_task_environment_;
};

ResetScreenTest::ResetScreenTest() {
  cros_settings_test_helper_.ReplaceDeviceSettingsProviderWithStub();
}

void ResetScreenTest::SetUnowned() {
  cros_settings_test_helper_.InstallAttributes()->Clear();
  cros_settings_test_helper_.InstallAttributes()->set_device_locked(false);
}

void ResetScreenTest::SetEnterpriseOwned() {
  cros_settings_test_helper_.InstallAttributes()->SetCloudManaged(
      kEnrollmentDomain, kEnrollmentId);
}

void ResetScreenTest::SetConsumerOwned() {
  cros_settings_test_helper_.InstallAttributes()->SetConsumerOwned();
}

void ResetScreenTest::SetPowerwashAllowedByPolicy(bool allowed) {
  cros_settings_test_helper_.Set(kDevicePowerwashAllowed, base::Value(allowed));
}

void ResetScreenTest::SetFreOn() {
  fake_statistics_provider_.SetMachineStatistic(
      ash::system::kCheckEnrollmentKey, "1");
}

void ResetScreenTest::SetFreOff() {
  fake_statistics_provider_.SetMachineStatistic(
      ash::system::kCheckEnrollmentKey, "0");
}

TEST_F(ResetScreenTest, CheckPowerwashAllowedConsumerOwned) {
  SetConsumerOwned();
  ExpectPowerwashAllowed(true);
}

TEST_F(ResetScreenTest, CheckPowerwashAllowedOnEnrolledDevice) {
  SetEnterpriseOwned();

  SetPowerwashAllowedByPolicy(true);
  ExpectPowerwashAllowed(true);

  SetPowerwashAllowedByPolicy(false);
  ExpectPowerwashAllowed(false);
}

// TODO(b/353731379): Remove when removing legacy state determination code.
TEST_F(ResetScreenTest, CheckPowerwashAllowedNotOwned) {
  base::test::ScopedCommandLine command_line;
  command_line.GetProcessCommandLine()->AppendSwitchASCII(
      ash::switches::kEnterpriseEnableUnifiedStateDetermination,
      policy::AutoEnrollmentTypeChecker::kUnifiedStateDeterminationNever);
  SetUnowned();

  SetFreOn();
  ExpectPowerwashAllowed(false);

  SetFreOff();
  ExpectPowerwashAllowed(true);
}

}  // namespace ash
