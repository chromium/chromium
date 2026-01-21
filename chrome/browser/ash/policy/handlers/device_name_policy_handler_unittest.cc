// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/handlers/device_name_policy_handler.h"

#include "ash/constants/ash_features.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/ash/settings/stub_cros_settings_provider.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "components/policy/core/common/cloud/test/policy_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

using DeviceNamePolicy = DeviceNamePolicyHandler::DeviceNamePolicy;

class DeviceNamePolicyHandlerTest : public testing::Test {
 public:
  DeviceNamePolicyHandlerTest() = default;

  // testing::Test
  void SetUp() override {
    testing::Test::SetUp();
    network_handler_test_helper_ =
        std::make_unique<ash::NetworkHandlerTestHelper>();
  }

  void TearDown() override { handler_.reset(); }

  // Sets kDeviceHostnameTemplate policy which will eventually cause
  // OnDeviceHostnamePropertyChangedAndMachineStatisticsLoaded() to run
  // and set the hostname_.
  void SetTemplate(const std::string& hostname_template) {
    scoped_testing_cros_settings_.device_settings()->Set(
        ash::kDeviceHostnameTemplate, base::Value(hostname_template));
    // Makes sure that template is set before continuing.
    base::RunLoop().RunUntilIdle();
  }

  void UnsetTemplate() {
    scoped_testing_cros_settings_.device_settings()->Set(
        ash::kDeviceHostnameTemplate, base::Value());
    // Makes sure that template is set before continuing.
    base::RunLoop().RunUntilIdle();
  }

  // Sets kDeviceHostnameUserConfigurable policy which will eventually cause
  // the class to stop making direct calls to NetworkStateHandler::SetHostname()
  void SetConfigurable(bool configurable) {
    // This should not have any effect because the hostname setting was removed.
    scoped_testing_cros_settings_.device_settings()->SetBoolean(
        ash::kDeviceHostnameUserConfigurable, configurable);
    // Makes sure that template is set before continuing.
    base::RunLoop().RunUntilIdle();
  }

  void UnsetConfigurable() {
    scoped_testing_cros_settings_.device_settings()->Set(
        ash::kDeviceHostnameUserConfigurable, base::Value());
    // Makes sure that template is set before continuing.
    base::RunLoop().RunUntilIdle();
  }

  void InitializeHandler(bool is_device_managed) {
    if (is_device_managed) {
      attributes_ = std::make_unique<ash::ScopedStubInstallAttributes>(
          ash::StubInstallAttributes::CreateCloudManaged(
              PolicyBuilder::kFakeDomain, PolicyBuilder::kFakeDeviceId));
    } else {
      attributes_ = std::make_unique<ash::ScopedStubInstallAttributes>(
          ash::StubInstallAttributes::CreateConsumerOwned());
    }

    handler_ = base::WrapUnique(new DeviceNamePolicyHandler(
        TestingBrowserProcess::GetGlobal()
            ->platform_part()
            ->browser_policy_connector_ash(),
        ash::CrosSettings::Get(), &fake_statistics_provider_,
        ash::NetworkHandler::Get()->network_state_handler()));

    base::RunLoop().RunUntilIdle();
  }

  std::unique_ptr<DeviceNamePolicyHandler> handler_;

 private:
  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<ash::NetworkHandlerTestHelper> network_handler_test_helper_;
  ash::ScopedTestingCrosSettings scoped_testing_cros_settings_;
  std::unique_ptr<ash::ScopedStubInstallAttributes> attributes_;
  ash::system::ScopedFakeStatisticsProvider fake_statistics_provider_;
};

// Verifies that for unmanaged devices the policy state is kNoPolicy by
// default and the hostname chosen by the administrator is nullopt.
TEST_F(DeviceNamePolicyHandlerTest, NoPoliciesUnmanagedDevice) {
  InitializeHandler(/*is_device_managed=*/false);

  EXPECT_EQ(DeviceNamePolicy::kNoPolicy,
            handler_->GetDeviceNamePolicyForTesting());

  // GetHostnameChosenByAdministrator() should therefore return null.
  const std::optional<std::string> hostname =
      handler_->GetHostnameChosenByAdministrator();
  EXPECT_FALSE(hostname);
}

// These tests below apply only to managed devices since unmanaged devices do
// not have any policies applied.

// Verifies that for managed devices the policy state is
// kPolicyHostnameNotConfigurable by default and the hostname chosen by the
// administrator is nullopt.
TEST_F(DeviceNamePolicyHandlerTest, NoPoliciesManagedDevice) {
  InitializeHandler(/*is_device_managed=*/true);

  EXPECT_EQ(DeviceNamePolicy::kPolicyHostnameNotConfigurable,
            handler_->GetDeviceNamePolicyForTesting());

  // GetHostnameChosenByAdministrator() should therefore return null.
  const std::optional<std::string> hostname =
      handler_->GetHostnameChosenByAdministrator();
  EXPECT_FALSE(hostname);
}

// Verifies that when |kDeviceHostnameTemplate| policy is set, the device name
// policy is set to |kPolicyHostnameChosenByAdmin| and is unaffected by the
// |kDeviceHostnameUserConfigurable| policy. Also verifies that the hostname
// is the one set by the template.
TEST_F(DeviceNamePolicyHandlerTest, DeviceHostnameTemplatePolicyOn) {
  InitializeHandler(/*is_device_managed=*/true);

  // Check that DeviceNamePolicy changes from kPolicyHostnameNotConfigurable
  // to kPolicyHostnameChosenByAdmin on setting template.
  EXPECT_EQ(DeviceNamePolicy::kPolicyHostnameNotConfigurable,
            handler_->GetDeviceNamePolicyForTesting());
  const std::string hostname_template = "chromebook";
  SetTemplate(hostname_template);
  DeviceNamePolicy after = handler_->GetDeviceNamePolicyForTesting();
  EXPECT_EQ(DeviceNamePolicy::kPolicyHostnameChosenByAdmin, after);
  // Check GetHostnameChosenByAdministrator() returns the expected hostname
  // value.
  const std::optional<std::string> hostname_chosen_by_administrator =
      handler_->GetHostnameChosenByAdministrator();
  EXPECT_EQ(hostname_chosen_by_administrator, hostname_template);

  // Setting kDeviceHostnameUserConfigurable policy should not affect the
  // DeviceNamePolicy because template is set.
  SetConfigurable(true);
  EXPECT_EQ(DeviceNamePolicy::kPolicyHostnameChosenByAdmin,
            handler_->GetDeviceNamePolicyForTesting());
  SetConfigurable(false);
  EXPECT_EQ(DeviceNamePolicy::kPolicyHostnameChosenByAdmin,
            handler_->GetDeviceNamePolicyForTesting());
}

// Verifies that when `kDeviceHostnameTemplate` policy is unset, the device name
// policy is reset to `kDeviceHostnameUserConfigurable`.
TEST_F(DeviceNamePolicyHandlerTest, DeviceHostnameTemplatePolicyUnset) {
  InitializeHandler(/*is_device_managed=*/true);

  const std::string hostname_template = "chromebook";
  SetTemplate(hostname_template);
  ASSERT_EQ(DeviceNamePolicy::kPolicyHostnameChosenByAdmin,
            handler_->GetDeviceNamePolicyForTesting());
  ASSERT_EQ(handler_->GetHostnameChosenByAdministrator(), hostname_template);

  UnsetTemplate();
  EXPECT_EQ(DeviceNamePolicy::kPolicyHostnameNotConfigurable,
            handler_->GetDeviceNamePolicyForTesting());
  EXPECT_FALSE(handler_->GetHostnameChosenByAdministrator());
}

// Verifies that when |kDeviceHostnameTemplate| policy is not set, setting
// kDeviceHostnameUserConfigurable policy should not change the
// DeviceNamePolicy.
TEST_F(DeviceNamePolicyHandlerTest,
       DeviceHostnameTemplatePolicyOffManagedDevices) {
  InitializeHandler(/*is_device_managed=*/true);
  EXPECT_EQ(DeviceNamePolicy::kPolicyHostnameNotConfigurable,
            handler_->GetDeviceNamePolicyForTesting());
  SetConfigurable(true);
  EXPECT_EQ(DeviceNamePolicy::kPolicyHostnameNotConfigurable,
            handler_->GetDeviceNamePolicyForTesting());
  SetConfigurable(false);
  EXPECT_EQ(DeviceNamePolicy::kPolicyHostnameNotConfigurable,
            handler_->GetDeviceNamePolicyForTesting());
  UnsetConfigurable();
  EXPECT_EQ(DeviceNamePolicy::kPolicyHostnameNotConfigurable,
            handler_->GetDeviceNamePolicyForTesting());
}

}  // namespace policy
