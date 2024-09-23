// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/storage_access_api/storage_access_header_service_factory.h"

#include <memory>

#include "chrome/browser/storage_access_api/storage_access_header_service.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/safe_browsing/core/common/features.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/public/cpp/features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace storage_access_api::trial {

class StorageAccessHeaderServiceFactoryTest : public testing::Test {
 public:
  StorageAccessHeaderServiceFactoryTest() = default;

  void SetUp() override {
    // TODO(crbug.com/362466866): Instead of disabling the
    // `kSafetyHubAbusiveNotificationRevocation` feature, find a stable
    // fix such that the tests still pass when the feature is enabled.
    features_.InitWithFeatures(
        {::features::kPersistentOriginTrials,
         network::features::kStorageAccessHeadersTrial},
        {safe_browsing::kSafetyHubAbusiveNotificationRevocation});
    profile_ = std::make_unique<TestingProfile>();
  }

  TestingProfile* GetProfile() { return profile_.get(); }

 private:
  base::test::ScopedFeatureList features_;
  content::BrowserTaskEnvironment env_;
  std::unique_ptr<TestingProfile> profile_;
};

TEST_F(StorageAccessHeaderServiceFactoryTest, CreatesService) {
  EXPECT_NE(StorageAccessHeaderServiceFactory::GetForProfile(GetProfile()),
            nullptr);
}

TEST_F(StorageAccessHeaderServiceFactoryTest,
       OffTheRecordProfile_IsOwnInstance) {
  StorageAccessHeaderService* service =
      StorageAccessHeaderServiceFactory::GetForProfile(
          GetProfile()->GetOriginalProfile());

  ASSERT_NE(service, nullptr);

  auto otr_profile_id = Profile::OTRProfileID::CreateUniqueForTesting();
  auto* otr_service = StorageAccessHeaderServiceFactory::GetForProfile(
      GetProfile()->GetOffTheRecordProfile(otr_profile_id, true));

  EXPECT_NE(otr_service, nullptr);
  EXPECT_NE(otr_service, service);
}

}  // namespace storage_access_api::trial
