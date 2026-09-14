// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/account_preview_data_service_factory.h"

#include "base/functional/bind.h"
#include "base/test/with_feature_override.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/test/base/testing_profile.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

class AccountPreviewDataServiceFactoryTest
    : public base::test::WithFeatureOverride,
      public testing::Test {
 public:
  AccountPreviewDataServiceFactoryTest()
      : base::test::WithFeatureOverride(switches::kEnableAccountPreviewData) {}

  static std::unique_ptr<KeyedService> BuildServiceInstance(
      content::BrowserContext* context) {
    return AccountPreviewDataServiceFactory::GetInstance()
        ->BuildServiceInstanceForBrowserContext(context);
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_P(AccountPreviewDataServiceFactoryTest, GetForProfile) {
  TestingProfile::Builder builder;
  builder.AddTestingFactory(
      AccountPreviewDataServiceFactory::GetInstance(),
      base::BindRepeating(
          &AccountPreviewDataServiceFactoryTest::BuildServiceInstance));
  std::unique_ptr<TestingProfile> profile = builder.Build();

  if (IsParamFeatureEnabled()) {
    EXPECT_TRUE(AccountPreviewDataServiceFactory::GetForProfile(profile.get()));
  } else {
    EXPECT_FALSE(
        AccountPreviewDataServiceFactory::GetForProfile(profile.get()));
  }
}

TEST_P(AccountPreviewDataServiceFactoryTest, NullWhileTestingByDefault) {
  TestingProfile profile;
  EXPECT_FALSE(AccountPreviewDataServiceFactory::GetForProfile(&profile));
}

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(AccountPreviewDataServiceFactoryTest);
