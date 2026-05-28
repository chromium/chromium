// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/site_cookie_provider/site_cookie_provider_service_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_profile.h"
#include "components/site_cookie_provider/site_cookie_provider_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace site_cookie_provider {

class SiteCookieProviderServiceFactoryTest : public testing::Test {
 protected:
  SiteCookieProviderServiceFactoryTest() = default;
  ~SiteCookieProviderServiceFactoryTest() override = default;

  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(SiteCookieProviderServiceFactoryTest, ServiceInstantiation) {
  TestingProfile::Builder builder;
  std::unique_ptr<TestingProfile> profile = builder.Build();

  // The factory should successfully construct the service for a regular
  // profile.
  SiteCookieProviderService* service =
      SiteCookieProviderServiceFactory::GetForProfile(profile.get());
  EXPECT_TRUE(service);
}

TEST_F(SiteCookieProviderServiceFactoryTest, ServiceNotCreatedForOffTheRecord) {
  TestingProfile::Builder builder;
  std::unique_ptr<TestingProfile> profile = builder.Build();

  // Create an OffTheRecord (Incognito) profile.
  Profile* otr_profile =
      profile->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  ASSERT_TRUE(otr_profile);

  // The factory should not construct the service for off-the-record profiles.
  SiteCookieProviderService* otr_service =
      SiteCookieProviderServiceFactory::GetForProfile(otr_profile);
  EXPECT_FALSE(otr_service);
}

}  // namespace site_cookie_provider
