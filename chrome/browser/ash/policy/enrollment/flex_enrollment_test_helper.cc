// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/enrollment/flex_enrollment_test_helper.h"

#include "ash/constants/ash_switches.h"
#include "chrome/browser/ash/policy/enrollment/auto_enrollment_type_checker.h"
#include "chromeos/ash/components/dbus/oobe_config/fake_oobe_configuration_client.h"
#include "chromeos/ash/components/dbus/oobe_config/oobe_configuration_client.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"

namespace policy::test {

const char kFlexEnrollmentToken[] = "test_flex_token";
const char kFlexEnrollmentTokenOobeConfig[] = R"({
  "flexToken": "test_flex_token"
})";

FlexEnrollmentTestHelper::FlexEnrollmentTestHelper(
    base::test::ScopedCommandLine* command_line,
    ash::system::FakeStatisticsProvider* statistics_provider)
    : command_line_(command_line), statistics_provider_(statistics_provider) {
  ash::OobeConfigurationClient::InitializeFake();
}

FlexEnrollmentTestHelper::~FlexEnrollmentTestHelper() {
  ash::OobeConfigurationClient::Shutdown();
}

void FlexEnrollmentTestHelper::SetUpFlexDevice() {
  command_line_->GetProcessCommandLine()->AppendSwitch(
      ash::switches::kRevenBranding);
  statistics_provider_->SetMachineStatistic(
      ash::system::kFirmwareTypeKey, ash::system::kFirmwareTypeValueNonchrome);
}

void FlexEnrollmentTestHelper::SetUpFlexEnrollmentTokenConfig(
    const char config[]) {
  static_cast<ash::FakeOobeConfigurationClient*>(
      ash::OobeConfigurationClient::Get())
      ->SetConfiguration(config);
  // Trigger propagation of token from FakeOobeConfigurationClient to
  // OobeConfiguration.
  oobe_configuration_.CheckConfiguration();
}

void FlexEnrollmentTestHelper::EnableFREOnFlex() {
  command_line_->GetProcessCommandLine()->AppendSwitchASCII(
      ash::switches::kEnterpriseEnableForcedReEnrollmentOnFlex,
      AutoEnrollmentTypeChecker::kForcedReEnrollmentAlways);
}
}  // namespace policy::test
