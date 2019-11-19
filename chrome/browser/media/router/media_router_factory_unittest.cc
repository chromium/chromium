// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "chrome/browser/media/router/media_router_factory.h"
#include "chrome/browser/media/router/test/mock_media_router.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media_router {

class MediaRouterFactoryTest : public testing::Test {
 protected:
  MediaRouterFactoryTest() {}
  ~MediaRouterFactoryTest() override {}

  void SetUp() override {
    MediaRouterFactory::GetInstance()->SetTestingFactory(
        profile(), base::BindRepeating(&MockMediaRouter::Create));
  }

  Profile* profile() { return &profile_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

TEST_F(MediaRouterFactoryTest, CreateForRegularProfile) {
  ASSERT_TRUE(MediaRouterFactory::GetApiForBrowserContext(profile()));
}

TEST_F(MediaRouterFactoryTest, CreateForIncognitoProfile) {
  Profile* incognito_profile = profile()->GetOffTheRecordProfile();
  ASSERT_TRUE(incognito_profile);

  // Makes sure a MediaRouter can be created from an incognito Profile.
  MediaRouter* router =
      MediaRouterFactory::GetApiForBrowserContext(incognito_profile);
  ASSERT_TRUE(router);

  // A Profile and its incognito Profile share the same MediaRouter instance.
  ASSERT_EQ(router, MediaRouterFactory::GetApiForBrowserContext(profile()));
}

TEST_F(MediaRouterFactoryTest, IncognitoBrowserContextShutdown) {
  // Creates an incognito profile.
  profile()->GetOffTheRecordProfile();
  MockMediaRouter* router = static_cast<MockMediaRouter*>(
      MediaRouterFactory::GetApiForBrowserContext(profile()));
  ASSERT_TRUE(router);
  EXPECT_CALL(*router, OnIncognitoProfileShutdown());
  profile()->DestroyOffTheRecordProfile();
}

}  // namespace media_router
