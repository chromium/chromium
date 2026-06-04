// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/site_cookie_provider/site_cookie_provider_service_factory.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_profile.h"
#include "components/site_cookie_provider/features.h"
#include "components/site_cookie_provider/site_cookie_provider_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace site_cookie_provider {
namespace {

using ::testing::IsNull;
using ::testing::NotNull;

class SiteCookieProviderServiceFactoryTestBase : public testing::Test {
 protected:
  SiteCookieProviderServiceFactoryTestBase() = default;
  ~SiteCookieProviderServiceFactoryTestBase() override = default;

  content::BrowserTaskEnvironment task_environment_;
};

class SiteCookieProviderServiceFactoryTest
    : public SiteCookieProviderServiceFactoryTestBase {
 protected:
  SiteCookieProviderServiceFactoryTest() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kSiteCookieProviderEnabled);
  }
  ~SiteCookieProviderServiceFactoryTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(SiteCookieProviderServiceFactoryTest, ServiceInstantiation) {
  TestingProfile::Builder builder;
  std::unique_ptr<TestingProfile> profile = builder.Build();

  // The factory should successfully construct the service for a regular
  // profile.
  SiteCookieProviderService* service =
      SiteCookieProviderServiceFactory::GetForProfile(profile.get());
  EXPECT_THAT(service, NotNull());
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
  EXPECT_THAT(otr_service, IsNull());
}

class SiteCookieProviderServiceFactoryDisabledTest
    : public SiteCookieProviderServiceFactoryTestBase {
 protected:
  SiteCookieProviderServiceFactoryDisabledTest() {
    scoped_feature_list_.InitAndDisableFeature(
        features::kSiteCookieProviderEnabled);
  }
  ~SiteCookieProviderServiceFactoryDisabledTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(SiteCookieProviderServiceFactoryDisabledTest,
       ServiceNotCreatedWhenFeatureDisabled) {
  TestingProfile::Builder builder;
  std::unique_ptr<TestingProfile> profile = builder.Build();

  SiteCookieProviderService* service =
      SiteCookieProviderServiceFactory::GetForProfile(profile.get());
  EXPECT_THAT(service, IsNull());
}

}  // namespace
}  // namespace site_cookie_provider
