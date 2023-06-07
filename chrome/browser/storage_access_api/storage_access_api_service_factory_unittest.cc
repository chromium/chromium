// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/storage_access_api/storage_access_api_service_factory.h"

#include "base/memory/raw_ptr.h"
#include "chrome/browser/storage_access_api/storage_access_api_service_impl.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

class StorageAccessAPIServiceFactoryTest : public testing::Test {
 public:
  StorageAccessAPIServiceFactoryTest() = default;

  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile("Profile");
  }

  void TearDown() override { profile_manager_.DeleteAllTestingProfiles(); }

  TestingProfile* profile() { return profile_; }

 private:
  content::BrowserTaskEnvironment env_;
  TestingProfileManager profile_manager_ =
      TestingProfileManager(TestingBrowserProcess::GetGlobal());
  raw_ptr<TestingProfile, DanglingUntriaged> profile_ = nullptr;
};

TEST_F(StorageAccessAPIServiceFactoryTest, RegularProfile_ServiceCreated) {
  EXPECT_NE(nullptr,
            StorageAccessAPIServiceFactory::GetForBrowserContext(profile()));
}

TEST_F(StorageAccessAPIServiceFactoryTest, OffTheRecordProfile_OwnInstance) {
  StorageAccessAPIServiceImpl* original_service =
      StorageAccessAPIServiceFactory::GetForBrowserContext(
          profile()->GetOriginalProfile());

  ASSERT_NE(nullptr, original_service);

  auto otr_profile_id = Profile::OTRProfileID::CreateUniqueForTesting();
  auto* otr_service = StorageAccessAPIServiceFactory::GetForBrowserContext(
      profile()->GetOffTheRecordProfile(otr_profile_id,
                                        /*create_if_needed=*/true));

  EXPECT_NE(nullptr, otr_service);
  EXPECT_NE(original_service, otr_service);
}
