// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_effects/media_effects_service_factory.h"

#include "components/user_prefs/test/test_browser_context_with_prefs.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

class MediaEffectsServiceFactoryTest : public testing::Test {
 protected:
  content::BrowserTaskEnvironment task_environment_;
  user_prefs::TestBrowserContextWithPrefs profile1_;
  user_prefs::TestBrowserContextWithPrefs profile2_;
};

TEST_F(MediaEffectsServiceFactoryTest,
       GetForBrowserContext_SameProfileReturnsSameService) {
  ASSERT_NE(&profile1_, &profile2_);
  EXPECT_EQ(MediaEffectsServiceFactory::GetForBrowserContext(&profile1_),
            MediaEffectsServiceFactory::GetForBrowserContext(&profile1_));
  EXPECT_EQ(MediaEffectsServiceFactory::GetForBrowserContext(&profile2_),
            MediaEffectsServiceFactory::GetForBrowserContext(&profile2_));
}

TEST_F(MediaEffectsServiceFactoryTest,
       GetForBrowserContext_DifferentProfileReturnsDifferentService) {
  ASSERT_NE(&profile1_, &profile2_);
  EXPECT_NE(MediaEffectsServiceFactory::GetForBrowserContext(&profile1_),
            MediaEffectsServiceFactory::GetForBrowserContext(&profile2_));
}

TEST_F(MediaEffectsServiceFactoryTest,
       GetForBrowserContext_IncognitoProfileReturnsDifferentService) {
  user_prefs::TestBrowserContextWithPrefs incognito_profile2;
  incognito_profile2.set_is_off_the_record(true);
  ASSERT_NE(&profile2_, &incognito_profile2);
  EXPECT_NE(
      MediaEffectsServiceFactory::GetForBrowserContext(&profile2_),
      MediaEffectsServiceFactory::GetForBrowserContext(&incognito_profile2));
}
