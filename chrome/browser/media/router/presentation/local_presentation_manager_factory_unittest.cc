// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "chrome/browser/media/router/presentation/local_presentation_manager_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media_router {

class LocalPresentationManagerFactoryTest : public testing::Test {
 protected:
  LocalPresentationManagerFactoryTest() {}
  ~LocalPresentationManagerFactoryTest() override {}

  Profile* profile() { return &profile_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

TEST_F(LocalPresentationManagerFactoryTest, CreateForRegularProfile) {
  ASSERT_TRUE(
      LocalPresentationManagerFactory::GetOrCreateForBrowserContext(profile()));
}

TEST_F(LocalPresentationManagerFactoryTest, CreateForOffTheRecordProfile) {
  Profile* incognito_profile = profile()->GetOffTheRecordProfile();
  ASSERT_TRUE(incognito_profile);

  // Makes sure a LocalPresentationManager can be created from an incognito
  // Profile.
  LocalPresentationManager* manager =
      LocalPresentationManagerFactory::GetOrCreateForBrowserContext(
          incognito_profile);
  ASSERT_TRUE(manager);

  // A Profile and its incognito Profile share the same
  // LocalPresentationManager instance.
  ASSERT_EQ(
      manager,
      LocalPresentationManagerFactory::GetOrCreateForBrowserContext(profile()));
}

}  // namespace media_router
