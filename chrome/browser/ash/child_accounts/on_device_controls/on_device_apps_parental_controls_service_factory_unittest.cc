// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/child_accounts/on_device_controls/on_device_apps_parental_controls_service_factory.h"

#include <memory>
#include <string>
#include <tuple>

#include "ash/constants/ash_features.h"
#include "base/containers/flat_map.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

// This test class is testing all possible feature configurations and is
// parametrized with three booleans:
// * whether `kAdditionalOnDeviceAppsParentalControls` feature is enabled
// * whether `kForceAdditionalOnDeviceAppsParentalControlsAllRegions` is enabled
// * whether feature is available in the device region
class OnDeviceAppsParentalControlsServiceFactoryTest
    : public testing::Test,
      public testing::WithParamInterface<std::tuple<bool, bool, bool>> {
 public:
  OnDeviceAppsParentalControlsServiceFactoryTest() {
    scoped_feature_list_.InitWithFeatureStates(
        {{features::kAdditionalOnDeviceAppsParentalControls,
          IsOnDeviceAppsParentalControlsEnabled()},
         {features::kForceAdditionalOnDeviceAppsParentalControlsAllRegions,
          IsOnDeviceAppsParentalControlsForceEnabled()}});

    SetDeviceRegion(IsOnDeviceAppsParentalControlsAvailableInRegion() ? "gp"
                                                                      : "ca");
  }

  OnDeviceAppsParentalControlsServiceFactoryTest(
      const OnDeviceAppsParentalControlsServiceFactoryTest&) = delete;
  OnDeviceAppsParentalControlsServiceFactoryTest& operator=(
      const OnDeviceAppsParentalControlsServiceFactoryTest&) = delete;

  ~OnDeviceAppsParentalControlsServiceFactoryTest() override = default;

 protected:
  bool IsOnDeviceAppsParentalControlsEnabled() const {
    return std::get<0>(GetParam());
  }

  bool IsOnDeviceAppsParentalControlsForceEnabled() const {
    return std::get<1>(GetParam());
  }

  bool IsOnDeviceAppsParentalControlsAvailableInRegion() const {
    return std::get<2>(GetParam());
    ;
  }

 private:
  // Sets device region in VPD.
  void SetDeviceRegion(const std::string& region) {
    statistics_provider_.SetMachineStatistic(ash::system::kRegionKey, region);
  }

  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  ash::system::ScopedFakeStatisticsProvider statistics_provider_;
};

TEST_P(OnDeviceAppsParentalControlsServiceFactoryTest,
       IsFeatureEnabledForRegularUser) {
  TestingProfile::Builder builder;
  std::unique_ptr<Profile> profile = builder.Build();

  if (IsOnDeviceAppsParentalControlsEnabled() &&
      (IsOnDeviceAppsParentalControlsAvailableInRegion() ||
       IsOnDeviceAppsParentalControlsForceEnabled())) {
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
                         testing::Combine(testing::Bool(),
                                          testing::Bool(),
                                          testing::Bool()));

}  // namespace ash
