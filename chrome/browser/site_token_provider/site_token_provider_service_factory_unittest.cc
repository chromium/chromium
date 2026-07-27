// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/site_token_provider/site_token_provider_service_factory.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_profile.h"
#include "components/site_token_provider/features.h"
#include "components/site_token_provider/site_token_provider_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace site_token_provider {
namespace {

using ::testing::IsNull;
using ::testing::NotNull;

class SiteTokenProviderServiceFactoryTestBase : public testing::Test {
 protected:
  SiteTokenProviderServiceFactoryTestBase() = default;
  ~SiteTokenProviderServiceFactoryTestBase() override = default;

  content::BrowserTaskEnvironment task_environment_;
};

class SiteTokenProviderServiceFactoryTest
    : public SiteTokenProviderServiceFactoryTestBase {
 protected:
  SiteTokenProviderServiceFactoryTest() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kSiteTokenProviderEnabled);
  }
  ~SiteTokenProviderServiceFactoryTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(SiteTokenProviderServiceFactoryTest, ServiceInstantiation) {
  TestingProfile::Builder builder;
  std::unique_ptr<TestingProfile> profile = builder.Build();

  // The factory should successfully construct the service for a regular
  // profile.
  SiteTokenProviderService* service =
      SiteTokenProviderServiceFactory::GetForProfile(profile.get());
  EXPECT_THAT(service, NotNull());
}

TEST_F(SiteTokenProviderServiceFactoryTest, ServiceNotCreatedForOffTheRecord) {
  TestingProfile::Builder builder;
  std::unique_ptr<TestingProfile> profile = builder.Build();

  // Create an OffTheRecord (Incognito) profile.
  Profile* otr_profile =
      profile->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  ASSERT_TRUE(otr_profile);

  // The factory should not construct the service for off-the-record profiles.
  SiteTokenProviderService* otr_service =
      SiteTokenProviderServiceFactory::GetForProfile(otr_profile);
  EXPECT_THAT(otr_service, IsNull());
}

class SiteTokenProviderServiceFactoryDisabledTest
    : public SiteTokenProviderServiceFactoryTestBase {
 protected:
  SiteTokenProviderServiceFactoryDisabledTest() {
    scoped_feature_list_.InitAndDisableFeature(
        features::kSiteTokenProviderEnabled);
  }
  ~SiteTokenProviderServiceFactoryDisabledTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(SiteTokenProviderServiceFactoryDisabledTest,
       ServiceNotCreatedWhenFeatureDisabled) {
  TestingProfile::Builder builder;
  std::unique_ptr<TestingProfile> profile = builder.Build();

  SiteTokenProviderService* service =
      SiteTokenProviderServiceFactory::GetForProfile(profile.get());
  EXPECT_THAT(service, IsNull());
}

}  // namespace
}  // namespace site_token_provider
