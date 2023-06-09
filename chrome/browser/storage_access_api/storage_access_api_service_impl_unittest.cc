// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/storage_access_api/storage_access_api_service_impl.h"

#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "chrome/browser/storage_access_api/storage_access_api_service_factory.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/features_generated.h"

namespace {
const char* kHostA = "a.test";
const char* kHostB = "b.test";
}  // namespace

class StorageAccessAPIServiceImplTest : public testing::Test {
 public:
  StorageAccessAPIServiceImplTest() = default;

  void SetUp() override {
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
    profile_ = profile_manager_->CreateTestingProfile("TestProfile");
    service_ = StorageAccessAPIServiceFactory::GetForBrowserContext(profile_);
    ASSERT_NE(service_, nullptr);
  }

  void TearDown() override {
    DCHECK(service_);
    // Even though we reassign this in SetUp, service may be persisted between
    // tests if the factory has already created a service for the testing
    // profile being used.
    profile_manager_->DeleteAllTestingProfiles();
    profile_manager_.reset();
  }

  content::BrowserTaskEnvironment& env() { return env_; }

 protected:
  Profile* profile() { return profile_; }
  StorageAccessAPIServiceImpl* service() { return service_; }

 private:
  content::BrowserTaskEnvironment env_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<TestingProfileManager> profile_manager_;
  raw_ptr<Profile, DanglingUntriaged> profile_;
  raw_ptr<StorageAccessAPIServiceImpl, DanglingUntriaged> service_;
};

TEST_F(StorageAccessAPIServiceImplTest, RenewPermissionGrant) {
  StorageAccessAPIServiceImpl* service =
      StorageAccessAPIServiceFactory::GetForBrowserContext(profile());

  ASSERT_NE(nullptr, service);

  // TODO(https://crbug.com/1450356): test grant renewal.
}

TEST_F(StorageAccessAPIServiceImplTest, RenewPermissionGrant_DailyCache) {
  StorageAccessAPIServiceImpl* service =
      StorageAccessAPIServiceFactory::GetForBrowserContext(profile());
  ASSERT_NE(nullptr, service);

  url::Origin origin_a(
      url::Origin::Create(GURL(base::StrCat({"https://", kHostA}))));
  url::Origin origin_b(
      url::Origin::Create(GURL(base::StrCat({"https://", kHostB}))));

  EXPECT_TRUE(service->RenewPermissionGrant(origin_a, origin_b));
  EXPECT_FALSE(service->RenewPermissionGrant(origin_a, origin_b));

  // The first cache reset should happen between 0-25 hours from test start.  (0
  // hours because the "next midnight" might have been in just a few minutes. 25
  // hours because today might have been the day that daylight savings time
  // ended.)
  env().FastForwardBy(base::Hours(25));

  EXPECT_TRUE(service->RenewPermissionGrant(origin_a, origin_b));
  EXPECT_FALSE(service->RenewPermissionGrant(origin_a, origin_b));

  // The next cache reset should happen 24 hours after the first reset.
  env().FastForwardBy(base::Days(1));

  EXPECT_TRUE(service->RenewPermissionGrant(origin_a, origin_b));
  EXPECT_FALSE(service->RenewPermissionGrant(origin_a, origin_b));
}

class StorageAccessAPIServiceImplWithoutRefreshTest
    : public StorageAccessAPIServiceImplTest {
 public:
  StorageAccessAPIServiceImplWithoutRefreshTest() {
    features_.InitAndEnableFeatureWithParameters(
        blink::features::kStorageAccessAPI,
        {
            {blink::features::kStorageAccessAPIRefreshGrantsOnUserInteraction
                 .name,
             "false"},
        });
  }

 private:
  base::test::ScopedFeatureList features_;
};

TEST_F(StorageAccessAPIServiceImplWithoutRefreshTest, NoPeriodicTasks) {
  StorageAccessAPIServiceImpl* service =
      StorageAccessAPIServiceFactory::GetForBrowserContext(profile());
  ASSERT_NE(nullptr, service);

  EXPECT_FALSE(service->IsTimerRunningForTesting());

  env().FastForwardBy(base::Hours(48));

  EXPECT_FALSE(service->IsTimerRunningForTesting());
}

TEST_F(StorageAccessAPIServiceImplWithoutRefreshTest,
       RenewPermissionGrant_AlwaysNoop) {
  StorageAccessAPIServiceImpl* service =
      StorageAccessAPIServiceFactory::GetForBrowserContext(profile());
  ASSERT_NE(nullptr, service);

  url::Origin origin_a(
      url::Origin::Create(GURL(base::StrCat({"https://", kHostA}))));
  url::Origin origin_b(
      url::Origin::Create(GURL(base::StrCat({"https://", kHostB}))));

  EXPECT_FALSE(service->RenewPermissionGrant(origin_a, origin_b));

  // The daily cache shouldn't make any difference here.
  env().FastForwardBy(base::Hours(25));

  EXPECT_FALSE(service->RenewPermissionGrant(origin_a, origin_b));
}
