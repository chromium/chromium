// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/ohttp_key_service_factory.h"

#include "chrome/browser/safe_browsing/test_safe_browsing_service.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/hashprefix_realtime/hash_realtime_utils.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

class OhttpKeyServiceFactoryTest : public testing::Test {
 protected:
  OhttpKeyServiceFactoryTest() = default;
  ~OhttpKeyServiceFactoryTest() override = default;
  void SetUp() override {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{kHashPrefixRealTimeLookups},
        /*disabled_features=*/{});
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
    ASSERT_TRUE(g_browser_process->profile_manager());

    sb_service_ =
        base::MakeRefCounted<safe_browsing::TestSafeBrowsingService>();
    TestingBrowserProcess::GetGlobal()->SetSafeBrowsingService(
        sb_service_.get());
    g_browser_process->safe_browsing_service()->Initialize();
  }
  void TearDown() override {
    TestingBrowserProcess::GetGlobal()->safe_browsing_service()->ShutDown();
    TestingBrowserProcess::GetGlobal()->SetSafeBrowsingService(nullptr);
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  base::test::ScopedFeatureList feature_list_;

 private:
  scoped_refptr<safe_browsing::SafeBrowsingService> sb_service_;
  hash_realtime_utils::GoogleChromeBrandingPretenderForTesting apply_branding_;
  OhttpKeyServiceAllowerForTesting allow_ohttp_key_service_;
};

#if BUILDFLAG(IS_ANDROID)
TEST_F(OhttpKeyServiceFactoryTest, DisabledForRegularProfiles) {
  TestingProfile* profile = profile_manager_->CreateTestingProfile("profile");
  EXPECT_EQ(nullptr, OhttpKeyServiceFactory::GetForProfile(profile));
}
#else
TEST_F(OhttpKeyServiceFactoryTest, EnabledForRegularProfiles) {
  TestingProfile* profile = profile_manager_->CreateTestingProfile("profile");
  EXPECT_NE(nullptr, OhttpKeyServiceFactory::GetForProfile(profile));
}

// Regression test requested in crbug.com/355577214.
TEST_F(OhttpKeyServiceFactoryTest,
       EnabledForRegularProfiles_HashRealTimeLookupsDisabled) {
  feature_list_.Reset();
  feature_list_.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{kHashPrefixRealTimeLookups});
  TestingProfile* profile = profile_manager_->CreateTestingProfile("profile");
  // The service should still be created even though HPRT lookups are disabled.
  // Instead, the OHTTP key service disables itself if HPRT lookups are
  // disabled.
  EXPECT_NE(nullptr, OhttpKeyServiceFactory::GetForProfile(profile));
}

TEST_F(OhttpKeyServiceFactoryTest, DisabledForIncognitoMode) {
  TestingProfile* profile = TestingProfile::Builder().BuildIncognito(
      profile_manager_->CreateTestingProfile("testing_profile"));
  EXPECT_EQ(nullptr, OhttpKeyServiceFactory::GetForProfile(profile));
}

TEST_F(OhttpKeyServiceFactoryTest, DisabledForGuestMode) {
  Profile* profile =
      profile_manager_->CreateGuestProfile()->GetPrimaryOTRProfile(
          /*create_if_needed=*/true);
  EXPECT_EQ(nullptr, OhttpKeyServiceFactory::GetForProfile(profile));
}
#endif

}  // namespace safe_browsing
