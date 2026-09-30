// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_navigation_gating_service_factory.h"

#include <memory>

#include "chrome/browser/devtools/devtools_navigation_gating_service.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class DevToolsNavigationGatingServiceFactoryTest : public testing::Test {
 protected:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(DevToolsNavigationGatingServiceFactoryTest, SameBrowserContext) {
  TestingProfile profile;

  DevToolsNavigationGatingService* service1 =
      DevToolsNavigationGatingServiceFactory::GetForBrowserContext(&profile);
  EXPECT_NE(service1, nullptr);

  DevToolsNavigationGatingService* service2 =
      DevToolsNavigationGatingServiceFactory::GetForBrowserContext(&profile);
  EXPECT_EQ(service1, service2);
}

TEST_F(DevToolsNavigationGatingServiceFactoryTest, UnrelatedBrowserContext) {
  TestingProfile profile1;
  TestingProfile profile2;

  DevToolsNavigationGatingService* service1 =
      DevToolsNavigationGatingServiceFactory::GetForBrowserContext(&profile1);
  ASSERT_NE(service1, nullptr);

  DevToolsNavigationGatingService* service2 =
      DevToolsNavigationGatingServiceFactory::GetForBrowserContext(&profile2);
  ASSERT_NE(service2, nullptr);

  EXPECT_NE(service1, service2);
}

TEST_F(DevToolsNavigationGatingServiceFactoryTest, OffTheRecordBrowserContext) {
  TestingProfile profile;
  Profile* otr_profile =
      profile.GetPrimaryOTRProfile(/*create_if_needed=*/true);

  DevToolsNavigationGatingService* regular_service =
      DevToolsNavigationGatingServiceFactory::GetForBrowserContext(&profile);
  ASSERT_NE(regular_service, nullptr);

  DevToolsNavigationGatingService* otr_service =
      DevToolsNavigationGatingServiceFactory::GetForBrowserContext(otr_profile);
  ASSERT_NE(otr_service, nullptr);

  EXPECT_NE(regular_service, otr_service);
  EXPECT_EQ(otr_service,
            DevToolsNavigationGatingServiceFactory::GetForBrowserContext(
                otr_profile));
}

}  // namespace
