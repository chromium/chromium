// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/launchpad/launchpad_service_factory.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/launchpad/features.h"
#include "chrome/browser/launchpad/launchpad_service.h"
#include "chrome/browser/personal_context/personal_context_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace launchpad {

class LaunchpadServiceFactoryTest : public testing::Test {
 public:
  LaunchpadServiceFactoryTest() = default;
  ~LaunchpadServiceFactoryTest() override = default;

 protected:
  std::unique_ptr<TestingProfile> BuildProfile() {
    TestingProfile::Builder builder;
    builder.AddTestingFactory(
        PersonalContextServiceFactory::GetInstance(),
        PersonalContextServiceFactory::GetDefaultFactory());
    return builder.Build();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(LaunchpadServiceFactoryTest, CreatesServiceWithFlagEnabled) {
  scoped_feature_list_.InitAndEnableFeature(features::kLaunchpad);
  std::unique_ptr<TestingProfile> profile = BuildProfile();
  EXPECT_NE(nullptr, LaunchpadServiceFactory::GetForProfile(profile.get()));
}

TEST_F(LaunchpadServiceFactoryTest, CreatesNoServiceWithFlagDisabled) {
  scoped_feature_list_.InitAndDisableFeature(features::kLaunchpad);
  std::unique_ptr<TestingProfile> profile = BuildProfile();
  EXPECT_EQ(nullptr, LaunchpadServiceFactory::GetForProfile(profile.get()));
}

TEST_F(LaunchpadServiceFactoryTest,
       CreatesNoServiceWithoutPersonalContextService) {
  scoped_feature_list_.InitAndEnableFeature(features::kLaunchpad);
  TestingProfile profile;
  EXPECT_EQ(nullptr, LaunchpadServiceFactory::GetForProfile(&profile));
}

TEST_F(LaunchpadServiceFactoryTest,
       DoesNotCreateServiceForIncognitoWithFlagEnabled) {
  scoped_feature_list_.InitAndEnableFeature(features::kLaunchpad);
  std::unique_ptr<TestingProfile> profile = BuildProfile();
  Profile* otr_profile = profile->GetOffTheRecordProfile(
      Profile::OTRProfileID::PrimaryID(), /*create_if_needed=*/true);
  EXPECT_EQ(nullptr, LaunchpadServiceFactory::GetForProfile(otr_profile));
}

}  // namespace launchpad
