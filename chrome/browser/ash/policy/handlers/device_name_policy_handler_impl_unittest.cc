// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/handlers/device_name_policy_handler_impl.h"

#include "ash/constants/ash_features.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/ash/settings/stub_cros_settings_provider.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "components/policy/core/common/cloud/test/policy_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {
namespace {

class FakeObserver : public DeviceNamePolicyHandler::Observer {
 public:
  FakeObserver() = default;
  ~FakeObserver() override = default;

  size_t num_calls() const { return num_calls_; }

  // DeviceNamePolicyHandler::Observer:
  void OnHostnamePolicyChanged() override { ++num_calls_; }

 private:
  size_t num_calls_ = 0;
};

}  // namespace

class DeviceNamePolicyHandlerImplTest : public testing::Test {
 public:
  DeviceNamePolicyHandlerImplTest() = default;

  // testing::Test
  void SetUp() override {
    testing::Test::SetUp();
    network_handler_test_helper_ =
        std::make_unique<ash::NetworkHandlerTestHelper>();
  }

  void TearDown() override {
    handler_->RemoveObserver(&fake_observer_);
    handler_.reset();
  }

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

  void InitializeHandler(bool is_hostname_setting_flag_enabled,
                         bool is_device_managed) {
    if (is_hostname_setting_flag_enabled)
      feature_list_.InitAndEnableFeature(ash::features::kEnableHostnameSetting);

    if (is_device_managed) {
      attributes_ = std::make_unique<ash::ScopedStubInstallAttributes>(
          ash::StubInstallAttributes::CreateCloudManaged(
              PolicyBuilder::kFakeDomain, PolicyBuilder::kFakeDeviceId));
    } else {
      attributes_ = std::make_unique<ash::ScopedStubInstallAttributes>(
          ash::StubInstallAttributes::CreateConsumerOwned());
    }

    handler_ = base::WrapUnique(new DeviceNamePolicyHandlerImpl(
        ash::CrosSettings::Get(), &fake_statistics_provider_,
        ash::NetworkHandler::Get()->network_state_handler()));
    handler_->AddObserver(&fake_observer_);
    base::RunLoop().RunUntilIdle();
  }

  size_t GetNumObserverCalls() const { return fake_observer_.num_calls(); }

  // Verifies that for unmanaged devices the policy state is kNoPolicy by
  // default and the hostname chosen by the administrator is nullopt. Flag state
  // does not matter.
  void VerifyDefaultStateUnmanagedDevice() {
    EXPECT_EQ(DeviceNamePolicyHandler::DeviceNamePolicy::kNoPolicy,
              handler_->GetDeviceNamePolicy());

    // GetHostnameChosenByAdministrator() should therefore return null.
    const std::optional<std::string> hostname =
        handler_->GetHostnameChosenByAdministrator();
    EXPECT_FALSE(hostname);
  }

  // Verifies that for managed devices the policy state is
  // kPolicyHostnameNotConfigurable by default and the hostname chosen by the
  // administrator is nullopt. Flag state does not matter.
  void VerifyDefaultStateManagedDevice() {
    EXPECT_EQ(DeviceNamePolicyHandler::DeviceNamePolicy::
                  kPolicyHostnameNotConfigurable,
              handler_->GetDeviceNamePolicy());

    // GetHostnameChosenByAdministrator() should therefore return null.
    const std::optional<std::string> hostname =
        handler_->GetHostnameChosenByAdministrator();
    EXPECT_FALSE(hostname);
  }

  // Verifies that when |kDeviceHostnameTemplate| policy is set, the device name
  // policy is set to |kPolicyHostnameChosenByAdmin| and is unaffected by the
  // |kDeviceHostnameUserConfigurable| policy. Also verifies that the hostname
  // is the one set by the template. Flag state does not matter.
  void VerifyStateWithAdminPolicy() {
    // Check that DeviceNamePolicy changes from kPolicyHostnameNotConfigurable
    // to kPolicyHostnameChosenByAdmin on setting template.
    EXPECT_EQ(DeviceNamePolicyHandler::DeviceNamePolicy::
                  kPolicyHostnameNotConfigurable,
              handler_->GetDeviceNamePolicy());
    const std::string hostname_template = "chromebook";
    SetTemplate(hostname_template);
    DeviceNamePolicyHandler::DeviceNamePolicy after =
        handler_->GetDeviceNamePolicy();
    EXPECT_EQ(
        DeviceNamePolicyHandler::DeviceNamePolicy::kPolicyHostnameChosenByAdmin,
        after);
    // Check GetHostnameChosenByAdministrator() returns the expected hostname
    // value.
    const std::optional<std::string> hostname_chosen_by_administrator =
        handler_->GetHostnameChosenByAdministrator();
    EXPECT_EQ(hostname_chosen_by_administrator, hostname_template);

    // Setting kDeviceHostnameUserConfigurable policy should not affect the
    // DeviceNamePolicy because template is set.
    SetConfigurable(true);
    EXPECT_EQ(
        DeviceNamePolicyHandler::DeviceNamePolicy::kPolicyHostnameChosenByAdmin,
        handler_->GetDeviceNamePolicy());
    SetConfigurable(false);
    EXPECT_EQ(
        DeviceNamePolicyHandler::DeviceNamePolicy::kPolicyHostnameChosenByAdmin,
        handler_->GetDeviceNamePolicy());
  }

  // Verifies the number of calls received by the observer for any changes in
  // |kDeviceHostnameTemplate| policy and hostname. Flag state does not matter.
  void VerifyObserverNumCalls() {
    // Neither hostname or policy changes on initialization of handler
    EXPECT_EQ(0u, GetNumObserverCalls());

    // Both hostname and policy change, hence observer should be notified once
    EXPECT_EQ(DeviceNamePolicyHandler::DeviceNamePolicy::
                  kPolicyHostnameNotConfigurable,
              handler_->GetDeviceNamePolicy());
    EXPECT_FALSE(handler_->GetHostnameChosenByAdministrator());
    std::string hostname_template = "template1";
    SetTemplate(hostname_template);
    EXPECT_EQ(
        DeviceNamePolicyHandler::DeviceNamePolicy::kPolicyHostnameChosenByAdmin,
        handler_->GetDeviceNamePolicy());
    EXPECT_EQ(hostname_template, handler_->GetHostnameChosenByAdministrator());
    EXPECT_EQ(1u, GetNumObserverCalls());

    // Hostname changes every time, hence observer should be notified each time.
    hostname_template = "template2";
    SetTemplate(hostname_template);
    EXPECT_EQ(hostname_template, handler_->GetHostnameChosenByAdministrator());
    EXPECT_EQ(2u, GetNumObserverCalls());
    hostname_template = "template3";
    SetTemplate(hostname_template);
    EXPECT_EQ(hostname_template, handler_->GetHostnameChosenByAdministrator());
    EXPECT_EQ(3u, GetNumObserverCalls());

    // Hostname is unchanged, hence observer should not be notified.
    const std::string const_template = "const_template";
    SetTemplate(const_template);
    EXPECT_EQ(const_template, handler_->GetHostnameChosenByAdministrator());
    EXPECT_EQ(4u, GetNumObserverCalls());
    SetTemplate(const_template);
    EXPECT_EQ(const_template, handler_->GetHostnameChosenByAdministrator());
    EXPECT_EQ(4u, GetNumObserverCalls());
  }

  std::unique_ptr<DeviceNamePolicyHandler> handler_;

 private:
  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<ash::NetworkHandlerTestHelper> network_handler_test_helper_;
  ash::ScopedTestingCrosSettings scoped_testing_cros_settings_;
  std::unique_ptr<ash::ScopedStubInstallAttributes> attributes_;
  ash::system::ScopedFakeStatisticsProvider fake_statistics_provider_;
  FakeObserver fake_observer_;
};

TEST_F(DeviceNamePolicyHandlerImplTest, NoPoliciesFlagOnManagedDevice) {
  InitializeHandler(/*is_hostname_setting_flag_enabled=*/true,
                    /*is_device_managed=*/true);
  VerifyDefaultStateManagedDevice();
}

TEST_F(DeviceNamePolicyHandlerImplTest, NoPoliciesFlagOnUnmanagedDevice) {
  InitializeHandler(/*is_hostname_setting_flag_enabled=*/true,
                    /*is_device_managed=*/false);
  VerifyDefaultStateUnmanagedDevice();
}

TEST_F(DeviceNamePolicyHandlerImplTest, NoPoliciesFlagOffManagedDevice) {
  InitializeHandler(/*is_hostname_setting_flag_enabled=*/false,
                    /*is_device_managed=*/true);
  VerifyDefaultStateManagedDevice();
}

TEST_F(DeviceNamePolicyHandlerImplTest, NoPoliciesFlagOffUnmanagedDevice) {
  InitializeHandler(/*is_hostname_setting_flag_enabled=*/false,
                    /*is_device_managed=*/false);
  VerifyDefaultStateUnmanagedDevice();
}

// The tests below apply only to managed devices since unmanaged devices do not
// have any policies applied.

TEST_F(DeviceNamePolicyHandlerImplTest, DeviceHostnameTemplatePolicyOnFlagOn) {
  InitializeHandler(/*is_hostname_setting_flag_enabled=*/true,
                    /*is_device_managed=*/true);
  VerifyStateWithAdminPolicy();
}

TEST_F(DeviceNamePolicyHandlerImplTest, DeviceHostnameTemplatePolicyOnFlagOff) {
  InitializeHandler(/*is_hostname_setting_flag_enabled=*/false,
                    /*is_device_managed=*/true);
  VerifyStateWithAdminPolicy();
}

// Verifies that when |kDeviceHostnameTemplate| policy is not set and flag
// is on, setting kDeviceHostnameUserConfigurable policy should change the
// DeviceNamePolicy.
TEST_F(DeviceNamePolicyHandlerImplTest, DeviceHostnameTemplatePolicyOffFlagOn) {
  InitializeHandler(/*is_hostname_setting_flag_enabled=*/true,
                    /*is_device_managed=*/true);
  EXPECT_EQ(
      DeviceNamePolicyHandler::DeviceNamePolicy::kPolicyHostnameNotConfigurable,
      handler_->GetDeviceNamePolicy());
  SetConfigurable(true);
  EXPECT_EQ(DeviceNamePolicyHandler::DeviceNamePolicy::
                kPolicyHostnameConfigurableByManagedUser,
            handler_->GetDeviceNamePolicy());
  SetConfigurable(false);
  EXPECT_EQ(
      DeviceNamePolicyHandler::DeviceNamePolicy::kPolicyHostnameNotConfigurable,
      handler_->GetDeviceNamePolicy());
  UnsetConfigurable();
  EXPECT_EQ(
      DeviceNamePolicyHandler::DeviceNamePolicy::kPolicyHostnameNotConfigurable,
      handler_->GetDeviceNamePolicy());
}

// Verifies that when |kDeviceHostnameTemplate| policy is not set and flag
// is off, setting kDeviceHostnameUserConfigurable policy should not change the
// DeviceNamePolicy.
TEST_F(DeviceNamePolicyHandlerImplTest,
       DeviceHostnameTemplatePolicyOffFlagOffManagedDevices) {
  InitializeHandler(/*is_hostname_setting_flag_enabled=*/false,
                    /*is_device_managed=*/true);
  EXPECT_EQ(
      DeviceNamePolicyHandler::DeviceNamePolicy::kPolicyHostnameNotConfigurable,
      handler_->GetDeviceNamePolicy());
  SetConfigurable(true);
  EXPECT_EQ(
      DeviceNamePolicyHandler::DeviceNamePolicy::kPolicyHostnameNotConfigurable,
      handler_->GetDeviceNamePolicy());
  SetConfigurable(false);
  EXPECT_EQ(
      DeviceNamePolicyHandler::DeviceNamePolicy::kPolicyHostnameNotConfigurable,
      handler_->GetDeviceNamePolicy());
  UnsetConfigurable();
  EXPECT_EQ(
      DeviceNamePolicyHandler::DeviceNamePolicy::kPolicyHostnameNotConfigurable,
      handler_->GetDeviceNamePolicy());
}

// Verifies that OnHostnamePolicyChanged() correctly notifies observer when
// hostname and/or policy changes, while the flag is on.
TEST_F(DeviceNamePolicyHandlerImplTest, ObserverTestsFlagOn) {
  InitializeHandler(/*is_hostname_setting_flag_enabled=*/true,
                    /*is_device_managed=*/true);
  VerifyObserverNumCalls();

  // Policy changes every time, hence observer should be notified each time.
  UnsetTemplate();
  EXPECT_EQ(
      DeviceNamePolicyHandler::DeviceNamePolicy::kPolicyHostnameNotConfigurable,
      handler_->GetDeviceNamePolicy());
  EXPECT_EQ(5u, GetNumObserverCalls());
  SetTemplate("hostname_template");
  EXPECT_EQ(
      DeviceNamePolicyHandler::DeviceNamePolicy::kPolicyHostnameChosenByAdmin,
      handler_->GetDeviceNamePolicy());
  EXPECT_EQ(6u, GetNumObserverCalls());
  UnsetTemplate();
  EXPECT_EQ(
      DeviceNamePolicyHandler::DeviceNamePolicy::kPolicyHostnameNotConfigurable,
      handler_->GetDeviceNamePolicy());
  EXPECT_EQ(7u, GetNumObserverCalls());
  SetConfigurable(true);
  EXPECT_EQ(DeviceNamePolicyHandler::DeviceNamePolicy::
                kPolicyHostnameConfigurableByManagedUser,
            handler_->GetDeviceNamePolicy());
  EXPECT_EQ(8u, GetNumObserverCalls());
  SetConfigurable(false);
  EXPECT_EQ(
      DeviceNamePolicyHandler::DeviceNamePolicy::kPolicyHostnameNotConfigurable,
      handler_->GetDeviceNamePolicy());
  EXPECT_EQ(9u, GetNumObserverCalls());
}

// Verifies that OnHostnamePolicyChanged() correctly notifies observer when
// hostname and/or policy changes, while the flag is off.
TEST_F(DeviceNamePolicyHandlerImplTest, ObserverTestsFlagOff) {
  InitializeHandler(/*is_hostname_setting_flag_enabled=*/false,
                    /*is_device_managed=*/true);
  VerifyObserverNumCalls();

  // Policy changes every time but observer should be notified only for changes
  // in the hostname template policy since flag is off.
  UnsetTemplate();
  EXPECT_EQ(
      DeviceNamePolicyHandler::DeviceNamePolicy::kPolicyHostnameNotConfigurable,
      handler_->GetDeviceNamePolicy());
  EXPECT_EQ(5u, GetNumObserverCalls());
  SetTemplate("hostname_template");
  EXPECT_EQ(
      DeviceNamePolicyHandler::DeviceNamePolicy::kPolicyHostnameChosenByAdmin,
      handler_->GetDeviceNamePolicy());
  EXPECT_EQ(6u, GetNumObserverCalls());
  UnsetTemplate();
  EXPECT_EQ(
      DeviceNamePolicyHandler::DeviceNamePolicy::kPolicyHostnameNotConfigurable,
      handler_->GetDeviceNamePolicy());
  EXPECT_EQ(7u, GetNumObserverCalls());
  SetConfigurable(false);
  EXPECT_EQ(
      DeviceNamePolicyHandler::DeviceNamePolicy::kPolicyHostnameNotConfigurable,
      handler_->GetDeviceNamePolicy());
  EXPECT_EQ(7u, GetNumObserverCalls());
  SetConfigurable(true);
  EXPECT_EQ(
      DeviceNamePolicyHandler::DeviceNamePolicy::kPolicyHostnameNotConfigurable,
      handler_->GetDeviceNamePolicy());
  EXPECT_EQ(7u, GetNumObserverCalls());
}

}  // namespace policy
