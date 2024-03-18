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
  ExtendedUpdatesParams MakeEligibleParams() const {
    return ExtendedUpdatesParams{
        .eol_passed = false,
        .extended_date_passed = true,
        .opt_in_required = true,
    };
  }

  base::test::ScopedFeatureList feature_list_;
  ScopedTestingCrosSettings cros_settings_;
  ash::ScopedStubInstallAttributes test_install_attributes_;
};

TEST_F(ExtendedUpdatesControllerTest, IsExtendedUpdatesOptInEligible_Eligible) {
  EXPECT_TRUE(
      IsExtendedUpdatesOptInEligible(profile_.get(), MakeEligibleParams()));
}

TEST_F(ExtendedUpdatesControllerTest,
       IsExtendedUpdatesOptInEligible_FeatureDisabled) {
  feature_list_.Reset();
  feature_list_.InitAndDisableFeature(features::kExtendedUpdatesOptInFeature);
  EXPECT_FALSE(
      IsExtendedUpdatesOptInEligible(profile_.get(), MakeEligibleParams()));
}

TEST_F(ExtendedUpdatesControllerTest, IsExtendedUpdatesOptInEligible_PastEol) {
  auto params = MakeEligibleParams();
  params.eol_passed = true;
  EXPECT_FALSE(IsExtendedUpdatesOptInEligible(profile_.get(), params));
}

TEST_F(ExtendedUpdatesControllerTest,
       IsExtendedUpdatesOptInEligible_BeforeExtendedDate) {
  auto params = MakeEligibleParams();
  params.extended_date_passed = false;
  EXPECT_FALSE(IsExtendedUpdatesOptInEligible(profile_.get(), params));
}

TEST_F(ExtendedUpdatesControllerTest,
       IsExtendedUpdatesOptInEligible_OptInNotRequired) {
  auto params = MakeEligibleParams();
  params.opt_in_required = false;
  EXPECT_FALSE(IsExtendedUpdatesOptInEligible(profile_.get(), params));
}

TEST_F(ExtendedUpdatesControllerTest,
       IsExtendedUpdatesOptInEligible_AlreadyOptedIn) {
  OwnerSettingsServiceAshFactory::GetForBrowserContext(profile_.get())
      ->SetBoolean(kDeviceExtendedAutoUpdateEnabled, true);
  EXPECT_FALSE(
      IsExtendedUpdatesOptInEligible(profile_.get(), MakeEligibleParams()));
}

TEST_F(ExtendedUpdatesControllerTest, IsExtendedUpdatesOptInEligible_NotOwner) {
  cros_settings_.device_settings()->SetCurrentUserIsOwner(false);
  EXPECT_FALSE(
      IsExtendedUpdatesOptInEligible(profile_.get(), MakeEligibleParams()));
}

TEST_F(ExtendedUpdatesControllerTest,
       IsExtendedUpdatesOptInEligible_IsManaged) {
  test_install_attributes_.Get()->SetCloudManaged("fake_domain", "fake_id");
  EXPECT_FALSE(
      IsExtendedUpdatesOptInEligible(profile_.get(), MakeEligibleParams()));
}

TEST_F(ExtendedUpdatesControllerTest,
       IsExtendedUpdatesOptedIn_NotOptedInByDefault) {
  EXPECT_FALSE(IsExtendedUpdatesOptedIn());
}

TEST_F(ExtendedUpdatesControllerTest, IsExtendedUpdatesOptedIn_OptedIn) {
  OwnerSettingsServiceAshFactory::GetForBrowserContext(profile_.get())
      ->SetBoolean(kDeviceExtendedAutoUpdateEnabled, true);
  EXPECT_TRUE(IsExtendedUpdatesOptedIn());
}

TEST_F(ExtendedUpdatesControllerTest, OptInExtendedUpdates_Success) {
  EXPECT_FALSE(IsExtendedUpdatesOptedIn());
  EXPECT_TRUE(OptInExtendedUpdates(profile_.get()));
  EXPECT_TRUE(IsExtendedUpdatesOptedIn());
}

TEST_F(ExtendedUpdatesControllerTest, OptInExtendedUpdates_FeatureDisabled) {
  feature_list_.Reset();
  feature_list_.InitAndDisableFeature(features::kExtendedUpdatesOptInFeature);
  EXPECT_FALSE(OptInExtendedUpdates(profile_.get()));
  EXPECT_FALSE(IsExtendedUpdatesOptedIn());
}

TEST_F(ExtendedUpdatesControllerTest, OptInExtendedUpdates_AlreadyOptedIn) {
  EXPECT_TRUE(OptInExtendedUpdates(profile_.get()));
  EXPECT_FALSE(OptInExtendedUpdates(profile_.get()));
  EXPECT_TRUE(IsExtendedUpdatesOptedIn());
}

TEST_F(ExtendedUpdatesControllerTest, OptInExtendedUpdates_NotOwner) {
  cros_settings_.device_settings()->SetCurrentUserIsOwner(false);
  EXPECT_FALSE(OptInExtendedUpdates(profile_.get()));
  EXPECT_FALSE(IsExtendedUpdatesOptedIn());
}

TEST_F(ExtendedUpdatesControllerTest, OptInExtendedUpdates_IsManaged) {
  test_install_attributes_.Get()->SetCloudManaged("fake_domain", "fake_id");
  EXPECT_FALSE(OptInExtendedUpdates(profile_.get()));
  EXPECT_FALSE(IsExtendedUpdatesOptedIn());
}

}  // namespace ash
