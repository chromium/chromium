// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/child_accounts/on_device_controls/app_controls_service_factory.h"

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

namespace ash::on_device_controls {

// This test class is testing all possible feature configurations for on-device
// apps controls. It is parametrized with three booleans:
// * whether `kOnDeviceAppControls` feature is enabled
// * whether `kForceOnDeviceAppControlsForAllRegions` is enabled
// * whether feature is available in the device region
class AppControlsServiceFactoryTest
    : public testing::Test,
      public testing::WithParamInterface<std::tuple<bool, bool, bool>> {
 public:
  AppControlsServiceFactoryTest() {
    scoped_feature_list_.InitWithFeatureStates(
        {{features::kOnDeviceAppControls, IsOnDeviceAppControlsEnabled()},
         {features::kForceOnDeviceAppControlsForAllRegions,
          IsOnDeviceAppControlsForceEnabled()}});

    SetDeviceRegion(IsOnDeviceAppControlsAvailableInRegion() ? "gp" : "ca");
  }

  AppControlsServiceFactoryTest(const AppControlsServiceFactoryTest&) = delete;
  AppControlsServiceFactoryTest& operator=(
      const AppControlsServiceFactoryTest&) = delete;

  ~AppControlsServiceFactoryTest() override = default;

 protected:
  bool IsOnDeviceAppControlsEnabled() const { return std::get<0>(GetParam()); }

  bool IsOnDeviceAppControlsForceEnabled() const {
    return std::get<1>(GetParam());
  }

  bool IsOnDeviceAppControlsAvailableInRegion() const {
    return std::get<2>(GetParam());
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

TEST_P(AppControlsServiceFactoryTest, IsFeatureEnabledForRegularUser) {
  TestingProfile::Builder builder;
  std::unique_ptr<Profile> profile = builder.Build();

  if ((IsOnDeviceAppControlsAvailableInRegion() &&
       IsOnDeviceAppControlsEnabled()) ||
      IsOnDeviceAppControlsForceEnabled()) {
    EXPECT_TRUE(AppControlsServiceFactory::IsOnDeviceAppControlsAvailable(
        profile.get()));
  } else {
    EXPECT_FALSE(AppControlsServiceFactory::IsOnDeviceAppControlsAvailable(
        profile.get()));
  }
}

TEST_P(AppControlsServiceFactoryTest, IsFeatureDisabledForManagedUser) {
  TestingProfile::Builder builder;
  builder.OverridePolicyConnectorIsManagedForTesting(true);
  std::unique_ptr<Profile> profile = builder.Build();

  EXPECT_FALSE(
      AppControlsServiceFactory::IsOnDeviceAppControlsAvailable(profile.get()));
}

TEST_P(AppControlsServiceFactoryTest, IsFeatureDisabledForSupervisedUser) {
  TestingProfile::Builder builder;
  builder.SetIsSupervisedProfile();
  std::unique_ptr<Profile> profile = builder.Build();

  EXPECT_FALSE(
      AppControlsServiceFactory::IsOnDeviceAppControlsAvailable(profile.get()));
}

INSTANTIATE_TEST_SUITE_P(,
                         AppControlsServiceFactoryTest,
                         testing::Combine(testing::Bool(),
                                          testing::Bool(),
                                          testing::Bool()));

}  // namespace ash::on_device_controls
