// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/child_accounts/on_device_controls/on_device_apps_parental_controls_service_factory.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

class OnDeviceAppsParentalControlsServiceFactoryTest
    : public testing::Test,
      public testing::WithParamInterface<bool> {
 public:
  OnDeviceAppsParentalControlsServiceFactoryTest() {
    if (IsOnDeviceAppsParentalControlsEnabled()) {
      scoped_feature_list.InitAndEnableFeature(
          features::kAdditionalOnDeviceAppsParentalControls);
    }
  }

  OnDeviceAppsParentalControlsServiceFactoryTest(
      const OnDeviceAppsParentalControlsServiceFactoryTest&) = delete;
  OnDeviceAppsParentalControlsServiceFactoryTest& operator=(
      const OnDeviceAppsParentalControlsServiceFactoryTest&) = delete;

  ~OnDeviceAppsParentalControlsServiceFactoryTest() override = default;

 protected:
  bool IsOnDeviceAppsParentalControlsEnabled() { return GetParam(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list;
};

TEST_P(OnDeviceAppsParentalControlsServiceFactoryTest,
       IsFeatureEnabledForRegularUser) {
  TestingProfile::Builder builder;
  std::unique_ptr<Profile> profile = builder.Build();

  if (IsOnDeviceAppsParentalControlsEnabled()) {
    EXPECT_TRUE(OnDeviceAppsParentalControlsServiceFactory::
                    IsOnDeviceAppsParentalControlsAvailable(profile.get()));
  } else {
    EXPECT_FALSE(OnDeviceAppsParentalControlsServiceFactory::
                     IsOnDeviceAppsParentalControlsAvailable(profile.get()));
  }
}

TEST_P(OnDeviceAppsParentalControlsServiceFactoryTest,
       IsFeatureDisabledForManagedUser) {
  TestingProfile::Builder builder;
  builder.OverridePolicyConnectorIsManagedForTesting(true);
  std::unique_ptr<Profile> profile = builder.Build();

  EXPECT_FALSE(OnDeviceAppsParentalControlsServiceFactory::
                   IsOnDeviceAppsParentalControlsAvailable(profile.get()));
}

TEST_P(OnDeviceAppsParentalControlsServiceFactoryTest,
       IsFeatureDisabledForSupervisedUser) {
  TestingProfile::Builder builder;
  builder.SetIsSupervisedProfile();
  std::unique_ptr<Profile> profile = builder.Build();

  EXPECT_FALSE(OnDeviceAppsParentalControlsServiceFactory::
                   IsOnDeviceAppsParentalControlsAvailable(profile.get()));
}

INSTANTIATE_TEST_SUITE_P(,
                         OnDeviceAppsParentalControlsServiceFactoryTest,
                         testing::Bool());

}  // namespace ash
