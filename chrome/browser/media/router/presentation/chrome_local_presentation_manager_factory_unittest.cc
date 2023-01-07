// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/presentation/chrome_local_presentation_manager_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media_router {

class ChromeLocalPresentationManagerFactoryTest : public testing::Test {
 protected:
  ChromeLocalPresentationManagerFactoryTest() = default;
  ~ChromeLocalPresentationManagerFactoryTest() override = default;

  void SetUp() override {
    testing::Test::SetUp();
    ChromeLocalPresentationManagerFactory::GetInstance();
  }

  Profile* profile() { return &profile_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

TEST_F(ChromeLocalPresentationManagerFactoryTest, CreateForRegularProfile) {
  EXPECT_TRUE(
      ChromeLocalPresentationManagerFactory::GetOrCreateForBrowserContext(
          profile()));
}

TEST_F(ChromeLocalPresentationManagerFactoryTest, CreateForIncognitoProfile) {
  Profile* incognito_profile =
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  ASSERT_TRUE(incognito_profile);

  // Makes sure a LocalPresentationManager can be created from an incognito
  // Profile.
  LocalPresentationManager* manager =
      ChromeLocalPresentationManagerFactory::GetOrCreateForBrowserContext(
          incognito_profile);
  EXPECT_TRUE(manager);

  // A Profile and its incognito Profile share the same
  // LocalPresentationManager instance.
  EXPECT_EQ(manager,
            ChromeLocalPresentationManagerFactory::GetOrCreateForBrowserContext(
                profile()));
}

}  // namespace media_router
