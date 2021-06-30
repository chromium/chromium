// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/handlers/device_name_policy_handler.h"

#include "base/test/task_environment.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/ash/settings/stub_cros_settings_provider.h"
#include "chromeos/network/network_handler_test_helper.h"
#include "chromeos/system/fake_statistics_provider.h"
#include "chromeos/tpm/stub_install_attributes.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

class DeviceNamePolicyHandlerTest : public testing::Test {
 public:
  DeviceNamePolicyHandlerTest() {}

  // testing::Test
  void SetUp() override {
    testing::Test::SetUp();
    network_handler_test_helper_ =
        std::make_unique<chromeos::NetworkHandlerTestHelper>();
  }

  void TearDown() override { handler_->Shutdown(); }

  // Sets kDeviceHostnameTemplate policy which will eventually cause
  // OnDeviceHostnamePropertyChangedAndMachineStatisticsLoaded() to run
  // and set the hostname_.
  void SetTemplate(const std::string& hostname_template) {
    scoped_testing_cros_settings_.device_settings()->Set(
        chromeos::kDeviceHostnameTemplate, base::Value(hostname_template));
    // Makes sure that template is set before continuing.
    base::RunLoop().RunUntilIdle();
  }

  void InitializeHandler() {
    handler_ = base::WrapUnique(new DeviceNamePolicyHandler(
        ash::CrosSettings::Get(), &fake_statistics_provider_));
    base::RunLoop().RunUntilIdle();
  }

  std::unique_ptr<DeviceNamePolicyHandler> handler_;

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<chromeos::NetworkHandlerTestHelper>
      network_handler_test_helper_;
  ash::ScopedTestingCrosSettings scoped_testing_cros_settings_;
  ash::ScopedStubInstallAttributes test_install_attributes_;
  chromeos::system::ScopedFakeStatisticsProvider fake_statistics_provider_;
};

// Check that device name policy set to kUnmanagedDevice by default.
TEST_F(DeviceNamePolicyHandlerTest, DeviceNamePolicyUnmanaged) {
  InitializeHandler();
  DeviceNamePolicyHandler::DeviceNamePolicy initial =
      handler_->GetDeviceNamePolicy();
  EXPECT_EQ(DeviceNamePolicyHandler::DeviceNamePolicy::kUnmanagedDevice,
            initial);

  // GetHostnameChosenByAdministrator() should therefore return null.
  const absl::optional<std::string> hostname =
      handler_->GetHostnameChosenByAdministrator();
  EXPECT_FALSE(hostname);
}

// Check outputs are correct when device name policy is set to
// kHostnameChosenByAdministrator.
TEST_F(DeviceNamePolicyHandlerTest, HostnameChosenByAdministrator) {
  InitializeHandler();
  // Check that DeviceNamePolicy changes from kUnmanagedDevice to
  // kHostnameChosenByAdministrator on setting template.
  DeviceNamePolicyHandler::DeviceNamePolicy initial =
      handler_->GetDeviceNamePolicy();
  EXPECT_EQ(DeviceNamePolicyHandler::DeviceNamePolicy::kUnmanagedDevice,
            initial);
  const std::string hostname_template = "chromebook";
  SetTemplate(hostname_template);
  DeviceNamePolicyHandler::DeviceNamePolicy after =
      handler_->GetDeviceNamePolicy();
  EXPECT_EQ(
      DeviceNamePolicyHandler::DeviceNamePolicy::kHostnameChosenByAdministrator,
      after);

  // Check that GetDeviceHostname() and GetHostnameChosenByAdministrator()
  // both return the expected hostname value.
  const absl::optional<std::string> hostname_chosen_by_administrator =
      handler_->GetHostnameChosenByAdministrator();
  const std::string device_hostname = handler_->GetDeviceHostname();
  EXPECT_EQ(hostname_chosen_by_administrator, "chromebook");
  EXPECT_EQ(device_hostname, "chromebook");
}

}  // namespace policy
