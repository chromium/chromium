// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/effects/media_effects_service_factory.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

class MediaEffectsServiceFactoryTest : public testing::Test {
 public:
  MediaEffectsServiceFactoryTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());
    profile1_ = profile_manager_.CreateTestingProfile("TestProfile1");
    profile2_ = profile_manager_.CreateTestingProfile("TestProfile2");
  }

  void TearDown() override {
    profile1_ = nullptr;
    profile2_ = nullptr;
    profile_manager_.DeleteAllTestingProfiles();
  }

 protected:
  raw_ptr<TestingProfile> profile1_;
  raw_ptr<TestingProfile> profile2_;

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
};

TEST_F(MediaEffectsServiceFactoryTest,
       GetForProfile_SameProfileReturnsSameService) {
  ASSERT_NE(profile1_, profile2_);
  EXPECT_EQ(MediaEffectsServiceFactory::GetForProfile(profile1_),
            MediaEffectsServiceFactory::GetForProfile(profile1_));
  EXPECT_EQ(MediaEffectsServiceFactory::GetForProfile(profile2_),
            MediaEffectsServiceFactory::GetForProfile(profile2_));
}

TEST_F(MediaEffectsServiceFactoryTest,
       GetForProfile_DifferentProfileReturnsDifferentService) {
  ASSERT_NE(profile1_, profile2_);
  EXPECT_NE(MediaEffectsServiceFactory::GetForProfile(profile1_),
            MediaEffectsServiceFactory::GetForProfile(profile2_));
}

TEST_F(MediaEffectsServiceFactoryTest,
       GetForProfile_IncognitoProfileReturnsDifferentService) {
  auto* incognito_profile2 =
      TestingProfile::Builder().BuildIncognito(profile2_);
  ASSERT_NE(profile2_, incognito_profile2);
  EXPECT_NE(MediaEffectsServiceFactory::GetForProfile(profile2_),
            MediaEffectsServiceFactory::GetForProfile(incognito_profile2));
}
