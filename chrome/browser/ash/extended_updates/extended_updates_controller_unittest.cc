// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/extended_updates/extended_updates_controller.h"

#include "ash/constants/ash_features.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash_factory.h"
#include "chrome/browser/ash/settings/device_settings_test_helper.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/account_id/account_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

class ExtendedUpdatesControllerTest : public DeviceSettingsTestBase {
 public:
  ExtendedUpdatesControllerTest() = default;
  ExtendedUpdatesControllerTest(const ExtendedUpdatesControllerTest&) = delete;
  ExtendedUpdatesControllerTest& operator=(
      const ExtendedUpdatesControllerTest&) = delete;
  ~ExtendedUpdatesControllerTest() override = default;

  void SetUp() override {
    DeviceSettingsTestBase::SetUp();

    feature_list_.InitAndEnableFeature(features::kExtendedUpdatesOptInFeature);

    owner_key_util_->ImportPrivateKeyAndSetPublicKey(
        device_policy_->GetSigningKey());
    InitOwner(
        AccountId::FromUserEmail(device_policy_->policy_data().username()),
        true);
    FlushDeviceSettings();
  }

 protected:
  ExtendedUpdatesController::Params MakeEligibleParams() const {
    return ExtendedUpdatesController::Params{
        .eol_passed = false,
        .extended_date_passed = true,
        .opt_in_required = true,
    };
  }

  ExtendedUpdatesController* controller() {
    return ExtendedUpdatesController::Get();
  }

  base::test::ScopedFeatureList feature_list_;
  ScopedTestingCrosSettings cros_settings_;
  ash::ScopedStubInstallAttributes test_install_attributes_;
};

TEST_F(ExtendedUpdatesControllerTest, IsOptInEligible_Eligible) {
  EXPECT_TRUE(
      controller()->IsOptInEligible(profile_.get(), MakeEligibleParams()));
}

TEST_F(ExtendedUpdatesControllerTest, IsOptInEligible_FeatureDisabled) {
  feature_list_.Reset();
  feature_list_.InitAndDisableFeature(features::kExtendedUpdatesOptInFeature);
  EXPECT_FALSE(
      controller()->IsOptInEligible(profile_.get(), MakeEligibleParams()));
}

TEST_F(ExtendedUpdatesControllerTest, IsOptInEligible_PastEol) {
  auto params = MakeEligibleParams();
  params.eol_passed = true;
  EXPECT_FALSE(controller()->IsOptInEligible(profile_.get(), params));
}

TEST_F(ExtendedUpdatesControllerTest, IsOptInEligible_BeforeExtendedDate) {
  auto params = MakeEligibleParams();
  params.extended_date_passed = false;
  EXPECT_FALSE(controller()->IsOptInEligible(profile_.get(), params));
}

TEST_F(ExtendedUpdatesControllerTest, IsOptInEligible_OptInNotRequired) {
  auto params = MakeEligibleParams();
  params.opt_in_required = false;
  EXPECT_FALSE(controller()->IsOptInEligible(profile_.get(), params));
}

TEST_F(ExtendedUpdatesControllerTest, IsOptInEligible_AlreadyOptedIn) {
  OwnerSettingsServiceAshFactory::GetForBrowserContext(profile_.get())
      ->SetBoolean(kDeviceExtendedAutoUpdateEnabled, true);
  EXPECT_FALSE(
      controller()->IsOptInEligible(profile_.get(), MakeEligibleParams()));
}

TEST_F(ExtendedUpdatesControllerTest, IsOptInEligible_NotOwner) {
  cros_settings_.device_settings()->SetCurrentUserIsOwner(false);
  EXPECT_FALSE(
      controller()->IsOptInEligible(profile_.get(), MakeEligibleParams()));
}

TEST_F(ExtendedUpdatesControllerTest, IsOptInEligible_IsManaged) {
  test_install_attributes_.Get()->SetCloudManaged("fake_domain", "fake_id");
  EXPECT_FALSE(
      controller()->IsOptInEligible(profile_.get(), MakeEligibleParams()));
}

TEST_F(ExtendedUpdatesControllerTest, IsOptedIn_NotOptedInByDefault) {
  EXPECT_FALSE(controller()->IsOptedIn());
}

TEST_F(ExtendedUpdatesControllerTest, IsOptedIn_OptedIn) {
  OwnerSettingsServiceAshFactory::GetForBrowserContext(profile_.get())
      ->SetBoolean(kDeviceExtendedAutoUpdateEnabled, true);
  EXPECT_TRUE(controller()->IsOptedIn());
}

TEST_F(ExtendedUpdatesControllerTest, OptIn_Success) {
  EXPECT_FALSE(controller()->IsOptedIn());
  EXPECT_TRUE(controller()->OptIn(profile_.get()));
  EXPECT_TRUE(controller()->IsOptedIn());
}

TEST_F(ExtendedUpdatesControllerTest, OptIn_FeatureDisabled) {
  feature_list_.Reset();
  feature_list_.InitAndDisableFeature(features::kExtendedUpdatesOptInFeature);
  EXPECT_FALSE(controller()->OptIn(profile_.get()));
  EXPECT_FALSE(controller()->IsOptedIn());
}

TEST_F(ExtendedUpdatesControllerTest, OptIn_AlreadyOptedIn) {
  EXPECT_TRUE(controller()->OptIn(profile_.get()));
  EXPECT_FALSE(controller()->OptIn(profile_.get()));
  EXPECT_TRUE(controller()->IsOptedIn());
}

TEST_F(ExtendedUpdatesControllerTest, OptIn_NotOwner) {
  cros_settings_.device_settings()->SetCurrentUserIsOwner(false);
  EXPECT_FALSE(controller()->OptIn(profile_.get()));
  EXPECT_FALSE(controller()->IsOptedIn());
}

TEST_F(ExtendedUpdatesControllerTest, OptIn_IsManaged) {
  test_install_attributes_.Get()->SetCloudManaged("fake_domain", "fake_id");
  EXPECT_FALSE(controller()->OptIn(profile_.get()));
  EXPECT_FALSE(controller()->IsOptedIn());
}

}  // namespace ash
