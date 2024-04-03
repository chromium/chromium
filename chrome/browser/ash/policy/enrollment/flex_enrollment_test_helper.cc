// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/enrollment/flex_enrollment_test_helper.h"

#include "ash/constants/ash_switches.h"
#include "chromeos/ash/components/dbus/oobe_config/fake_oobe_configuration_client.h"
#include "chromeos/ash/components/dbus/oobe_config/oobe_configuration_client.h"

namespace policy::test {

const char kFlexEnrollmentToken[] = "test_flex_token";
const char kFlexEnrollmentTokenOobeConfig[] = R"({
  "flexToken": "test_flex_token"
})";

FlexEnrollmentTestHelper::FlexEnrollmentTestHelper(
    base::test::ScopedCommandLine* command_line)
    : command_line_(command_line) {
  ash::OobeConfigurationClient::InitializeFake();
}

FlexEnrollmentTestHelper::~FlexEnrollmentTestHelper() {
  ash::OobeConfigurationClient::Shutdown();
}

void FlexEnrollmentTestHelper::SetUpFlexDevice() {
  command_line_->GetProcessCommandLine()->AppendSwitch(
      ash::switches::kRevenBranding);
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
}  // namespace policy::test
