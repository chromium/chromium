// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/account_preview_data_service_factory.h"

#include "base/test/with_feature_override.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/test/base/testing_profile.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class AccountPreviewDataServiceFactoryTest
    : public base::test::WithFeatureOverride,
      public testing::Test {
 public:
  AccountPreviewDataServiceFactoryTest()
      : base::test::WithFeatureOverride(switches::kEnableAccountPreviewData) {}

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

TEST_P(AccountPreviewDataServiceFactoryTest, GetForProfile) {
  if (IsParamFeatureEnabled()) {
    EXPECT_TRUE(AccountPreviewDataServiceFactory::GetForProfile(&profile_));
  } else {
    EXPECT_FALSE(AccountPreviewDataServiceFactory::GetForProfile(&profile_));
  }
}

TEST_P(AccountPreviewDataServiceFactoryTest, GetForProfile_SigninDisallowed) {
  auto prefs = std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
  RegisterUserProfilePrefs(prefs->registry());
  prefs->SetUserPref(prefs::kSigninAllowed, base::Value(false));
  prefs->SetUserPref(prefs::kSigninAllowedOnNextStartup, base::Value(false));

  TestingProfile::Builder builder;
  builder.SetPrefService(std::move(prefs));
  std::unique_ptr<TestingProfile> profile = builder.Build();
  EXPECT_FALSE(AccountPreviewDataServiceFactory::GetForProfile(profile.get()));
}

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(AccountPreviewDataServiceFactoryTest);

}  // namespace
