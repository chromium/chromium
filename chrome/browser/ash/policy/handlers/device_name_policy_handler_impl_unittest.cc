// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/handlers/device_name_policy_handler_impl.h"

#include "ash/constants/ash_features.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/ash/settings/stub_cros_settings_provider.h"
#include "chromeos/network/network_handler_test_helper.h"
#include "chromeos/system/fake_statistics_provider.h"
#include "chromeos/tpm/stub_install_attributes.h"
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
        std::make_unique<chromeos::NetworkHandlerTestHelper>();
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
        chromeos::kDeviceHostnameTemplate, base::Value(hostname_template));
    // Makes sure that template is set before continuing.
    base::RunLoop().RunUntilIdle();
  }

  void UnsetTemplate() {
    scoped_testing_cros_settings_.device_settings()->Set(
        chromeos::kDeviceHostnameTemplate, base::Value());
    // Makes sure that template is set before continuing.
    base::RunLoop().RunUntilIdle();
  }

  // Sets kDeviceHostnameUserConfigurable policy which will eventually cause
  // the class to stop making direct calls to NetworkStateHandler::SetHostname()
  void SetConfigurable(bool configurable) {
    scoped_testing_cros_settings_.device_settings()->SetBoolean(
        chromeos::kDeviceHostnameUserConfigurable, configurable);
    // Makes sure that template is set before continuing.
    base::RunLoop().RunUntilIdle();
  }

  void UnsetConfigurable() {
    scoped_testing_cros_settings_.device_settings()->Set(
        chromeos::kDeviceHostnameUserConfigurable, base::Value());
    // Makes sure that template is set before continuing.
    base::RunLoop().RunUntilIdle();
  }

  void InitializeHandler(bool is_hostname_setting_flag_enabled) {
    if (is_hostname_setting_flag_enabled)
      feature_list_.InitAndEnableFeature(ash::features::kEnableHostnameSetting);
    handler_ = base::WrapUnique(new DeviceNamePolicyHandlerImpl(
        ash::CrosSettings::Get(), &fake_statistics_provider_,
        chromeos::NetworkHandler::Get()->network_state_handler()));
    handler_->AddObserver(&fake_observer_);
    base::RunLoop().RunUntilIdle();
  }

  size_t GetNumObserverCalls() const { return fake_observer_.num_calls(); }

  // Verifies that when no policies are active, the policy state is kNoPolicy
  // and the hostname chosen by the administrator is nullopt. Flag state does
  // not matter.
  void VerifyStateWithNoPolicies() {
    EXPECT_EQ(DeviceNamePolicyHandler::DeviceNamePolicy::kNoPolicy,
              handler_->GetDeviceNamePolicy());

    // GetHostnameChosenByAdministrator() should therefore return null.
    const absl::optional<std::string> hostname =
        handler_->GetHostnameChosenByAdministrator();
    EXPECT_FALSE(hostname);
  }

  // Verifies that when |kDeviceHostnameTemplate| policy is set, the device name
  // policy is set to |kPolicyHostnameChosenByAdmin| and is unaffected by the
  // |kDeviceHostnameUserConfigurable| policy. Also verifies that the hostname
  // is the one set by the template. Flag state does not matter.
  void VerifyStateWithAdminPolicy() {
    // Check that DeviceNamePolicy changes from kNoPolicy to
    // kPolicyHostnameChosenByAdmin on setting template.
    EXPECT_EQ(DeviceNamePolicyHandler::DeviceNamePolicy::kNoPolicy,
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
    const absl::optional<std::string> hostname_chosen_by_administrator =
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
    EXPECT_EQ(DeviceNamePolicyHandler::DeviceNamePolicy::kNoPolicy,
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
  std::unique_ptr<chromeos::NetworkHandlerTestHelper>
      network_handler_test_helper_;
  ash::ScopedTestingCrosSettings scoped_testing_cros_settings_;
  ash::ScopedStubInstallAttributes test_install_attributes_;
  chromeos::system::ScopedFakeStatisticsProvider fake_statistics_provider_;
  FakeObserver fake_observer_;
};

TEST_F(DeviceNamePolicyHandlerImplTest, NoPoliciesFlagOn) {
  InitializeHandler(/*is_hostname_setting_flag_enabled=*/true);
  VerifyStateWithNoPolicies();
}

TEST_F(DeviceNamePolicyHandlerImplTest, NoPoliciesFlagOff) {
  InitializeHandler(/*is_hostname_setting_flag_enabled=*/false);
  VerifyStateWithNoPolicies();
}

TEST_F(DeviceNamePolicyHandlerImplTest, DeviceHostnameTemplatePolicyOnFlagOn) {
  InitializeHandler(/*is_hostname_setting_flag_enabled=*/true);
  VerifyStateWithAdminPolicy();
}

TEST_F(DeviceNamePolicyHandlerImplTest, DeviceHostnameTemplatePolicyOnFlagOff) {
  InitializeHandler(/*is_hostname_setting_flag_enabled=*/false);
  VerifyStateWithAdminPolicy();
}

// Verifies that when |kDeviceHostnameTemplate| policy is not set and flag
// is on, setting kDeviceHostnameUserConfigurable policy should change the
// DeviceNamePolicy.
TEST_F(DeviceNamePolicyHandlerImplTest, DeviceHostnameTemplatePolicyOffFlagOn) {
  InitializeHandler(/*is_hostname_setting_flag_enabled=*/true);
  EXPECT_EQ(DeviceNamePolicyHandler::DeviceNamePolicy::kNoPolicy,
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
  EXPECT_EQ(DeviceNamePolicyHandler::DeviceNamePolicy::kNoPolicy,
            handler_->GetDeviceNamePolicy());
}

// Verifies that when |kDeviceHostnameTemplate| policy is not set and flag
// is off, setting kDeviceHostnameUserConfigurable policy should not change the
// DeviceNamePolicy.
TEST_F(DeviceNamePolicyHandlerImplTest,
       DeviceHostnameTemplatePolicyOffFlagOff) {
  InitializeHandler(/*is_hostname_setting_flag_enabled=*/false);
  EXPECT_EQ(DeviceNamePolicyHandler::DeviceNamePolicy::kNoPolicy,
            handler_->GetDeviceNamePolicy());
  SetConfigurable(true);
  EXPECT_EQ(DeviceNamePolicyHandler::DeviceNamePolicy::kNoPolicy,
            handler_->GetDeviceNamePolicy());
  SetConfigurable(false);
  EXPECT_EQ(DeviceNamePolicyHandler::DeviceNamePolicy::kNoPolicy,
            handler_->GetDeviceNamePolicy());
  UnsetConfigurable();
  EXPECT_EQ(DeviceNamePolicyHandler::DeviceNamePolicy::kNoPolicy,
            handler_->GetDeviceNamePolicy());
}

// Verifies that OnHostnamePolicyChanged() correctly notifies observer when
// hostname and/or policy changes, while the flag is on.
TEST_F(DeviceNamePolicyHandlerImplTest, ObserverTestsFlagOn) {
  InitializeHandler(/*is_hostname_setting_flag_enabled=*/true);
  VerifyObserverNumCalls();

  // Policy changes every time, hence observer should be notified each time.
  UnsetTemplate();
  EXPECT_EQ(DeviceNamePolicyHandler::DeviceNamePolicy::kNoPolicy,
            handler_->GetDeviceNamePolicy());
  EXPECT_EQ(5u, GetNumObserverCalls());
  SetTemplate("hostname_template");
  EXPECT_EQ(
      DeviceNamePolicyHandler::DeviceNamePolicy::kPolicyHostnameChosenByAdmin,
      handler_->GetDeviceNamePolicy());
  EXPECT_EQ(6u, GetNumObserverCalls());
  UnsetTemplate();
  EXPECT_EQ(DeviceNamePolicyHandler::DeviceNamePolicy::kNoPolicy,
            handler_->GetDeviceNamePolicy());
  EXPECT_EQ(7u, GetNumObserverCalls());
  SetConfigurable(false);
  EXPECT_EQ(
      DeviceNamePolicyHandler::DeviceNamePolicy::kPolicyHostnameNotConfigurable,
      handler_->GetDeviceNamePolicy());
  EXPECT_EQ(8u, GetNumObserverCalls());
  SetConfigurable(true);
  EXPECT_EQ(DeviceNamePolicyHandler::DeviceNamePolicy::
                kPolicyHostnameConfigurableByManagedUser,
            handler_->GetDeviceNamePolicy());
  EXPECT_EQ(9u, GetNumObserverCalls());
}

// Verifies that OnHostnamePolicyChanged() correctly notifies observer when
// hostname and/or policy changes, while the flag is off.
TEST_F(DeviceNamePolicyHandlerImplTest, ObserverTestsFlagOff) {
  InitializeHandler(/*is_hostname_setting_flag_enabled=*/false);
  VerifyObserverNumCalls();

  // Policy changes every time but observer should be notified only for changes
  // in the hostname template policy since flag is off.
  UnsetTemplate();
  EXPECT_EQ(DeviceNamePolicyHandler::DeviceNamePolicy::kNoPolicy,
            handler_->GetDeviceNamePolicy());
  EXPECT_EQ(5u, GetNumObserverCalls());
  SetTemplate("hostname_template");
  EXPECT_EQ(
      DeviceNamePolicyHandler::DeviceNamePolicy::kPolicyHostnameChosenByAdmin,
      handler_->GetDeviceNamePolicy());
  EXPECT_EQ(6u, GetNumObserverCalls());
  UnsetTemplate();
  EXPECT_EQ(DeviceNamePolicyHandler::DeviceNamePolicy::kNoPolicy,
            handler_->GetDeviceNamePolicy());
  EXPECT_EQ(7u, GetNumObserverCalls());
  SetConfigurable(false);
  EXPECT_EQ(DeviceNamePolicyHandler::DeviceNamePolicy::kNoPolicy,
            handler_->GetDeviceNamePolicy());
  EXPECT_EQ(7u, GetNumObserverCalls());
  SetConfigurable(true);
  EXPECT_EQ(DeviceNamePolicyHandler::DeviceNamePolicy::kNoPolicy,
            handler_->GetDeviceNamePolicy());
  EXPECT_EQ(7u, GetNumObserverCalls());
}

}  // namespace policy
