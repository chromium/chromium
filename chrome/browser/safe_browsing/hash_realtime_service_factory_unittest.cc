// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/hash_realtime_service_factory.h"

#include "chrome/browser/safe_browsing/test_safe_browsing_service.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

class HashRealTimeServiceFactoryTest : public testing::Test {
 protected:
  HashRealTimeServiceFactoryTest() = default;
  ~HashRealTimeServiceFactoryTest() override = default;
  void SetUp() override {
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

 private:
  scoped_refptr<safe_browsing::SafeBrowsingService> sb_service_;
};

TEST_F(HashRealTimeServiceFactoryTest, EnabledForRegularProfiles) {
  TestingProfile* profile = profile_manager_->CreateTestingProfile("profile");
  EXPECT_NE(nullptr, HashRealTimeServiceFactory::GetForProfile(profile));
}

TEST_F(HashRealTimeServiceFactoryTest, DisabledForIncognitoMode) {
  TestingProfile* profile = TestingProfile::Builder().BuildIncognito(
      profile_manager_->CreateTestingProfile("testing_profile"));
  EXPECT_EQ(nullptr, HashRealTimeServiceFactory::GetForProfile(profile));
}

TEST_F(HashRealTimeServiceFactoryTest, DisabledForGuestMode) {
  Profile* profile =
      profile_manager_->CreateGuestProfile()->GetPrimaryOTRProfile(
          /*create_if_needed=*/true);
  EXPECT_EQ(nullptr, HashRealTimeServiceFactory::GetForProfile(profile));
}

}  // namespace safe_browsing
