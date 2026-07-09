// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/v5_search_hashes_cache_factory.h"

#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

class V5SearchHashesCacheFactoryTest : public testing::Test {
 protected:
  void SetUp() override {
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
};

TEST_F(V5SearchHashesCacheFactoryTest, EnabledForRegularProfiles) {
  TestingProfile* profile =
      profile_manager_->CreateTestingProfile("testing_profile");

  EXPECT_NE(nullptr, V5SearchHashesCacheFactory::GetForProfile(profile));
}

TEST_F(V5SearchHashesCacheFactoryTest, DisabledForIncognitoMode) {
  TestingProfile* profile =
      profile_manager_->CreateTestingProfile("testing_profile");
  Profile* incognito_profile =
      profile->GetPrimaryOTRProfile(/*create_if_needed=*/true);

  EXPECT_EQ(nullptr,
            V5SearchHashesCacheFactory::GetForProfile(incognito_profile));
}

}  // namespace safe_browsing
