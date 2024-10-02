// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/enrollment/enrollment_test_helper.h"

#include "ash/constants/ash_switches.h"
#include "chrome/browser/ash/login/configuration_keys.h"
#include "chrome/browser/ash/policy/enrollment/auto_enrollment_type_checker.h"
#include "chromeos/ash/components/dbus/oobe_config/fake_oobe_configuration_client.h"
#include "chromeos/ash/components/dbus/oobe_config/oobe_configuration_client.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"

namespace policy::test {

const char kEnrollmentToken[] = "test_enrollment_token";
const char kEnrollmentTokenOobeConfig[] = R"({
  "enrollmentToken": "test_enrollment_token"
})";

EnrollmentTestHelper::EnrollmentTestHelper(
    base::test::ScopedCommandLine* command_line,
    ash::system::FakeStatisticsProvider* statistics_provider)
    : command_line_(command_line), statistics_provider_(statistics_provider) {
  ash::OobeConfigurationClient::InitializeFake();
}

EnrollmentTestHelper::~EnrollmentTestHelper() {
  ash::OobeConfigurationClient::Shutdown();
}

void EnrollmentTestHelper::SetUpNonchromeDevice() {
  statistics_provider_->SetMachineStatistic(
      ash::system::kFirmwareTypeKey, ash::system::kFirmwareTypeValueNonchrome);
}

void EnrollmentTestHelper::SetUpFlexDevice() {
  SetUpNonchromeDevice();
  command_line_->GetProcessCommandLine()->AppendSwitch(
      ash::switches::kRevenBranding);
}

void EnrollmentTestHelper::SetUpEnrollmentTokenConfig(
    const char config[]) {
  static_cast<ash::FakeOobeConfigurationClient*>(
      ash::OobeConfigurationClient::Get())
      ->SetConfiguration(config);
  // Trigger propagation of token from FakeOobeConfigurationClient to
  // OobeConfiguration.
  oobe_configuration_.CheckConfiguration();
}

void EnrollmentTestHelper::DisableFREOnFlex() {
  command_line_->GetProcessCommandLine()->AppendSwitchASCII(
      ash::switches::kEnterpriseEnableForcedReEnrollmentOnFlex,
      AutoEnrollmentTypeChecker::kForcedReEnrollmentNever);
}

void EnrollmentTestHelper::EnableFREOnFlex() {
  // This is a no-op right now, but we keep this in case we need to revert
  // this CL.
}

const std::string*
EnrollmentTestHelper::GetEnrollmentTokenFromOobeConfiguration() {
  // First CheckConfiguration() so OobeConfiguration is updated with the latest
  // values from FakeOobeConfigurationClient.
  oobe_configuration_.CheckConfiguration();
  return oobe_configuration_.configuration().FindString(
      ash::configuration::kEnrollmentToken);
}

}  // namespace policy::test
