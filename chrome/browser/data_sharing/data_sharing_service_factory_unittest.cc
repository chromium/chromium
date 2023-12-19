// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/data_sharing/data_sharing_service_factory.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/testing_profile.h"
#include "components/data_sharing/internal/data_sharing_service_impl.h"
#include "components/data_sharing/internal/empty_data_sharing_service.h"
#include "components/data_sharing/public/data_sharing_service.h"
#include "components/data_sharing/public/features.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace data_sharing {
namespace {

class DataSharingServiceFactoryTest : public testing::Test {
 protected:
  DataSharingServiceFactoryTest() = default;

  ~DataSharingServiceFactoryTest() override = default;

  void InitService(bool enable_feature) {
    profile_ = TestingProfile::Builder().Build();
    if (enable_feature) {
      scoped_feature_list_.InitWithFeaturesAndParameters(
          {{data_sharing::features::kDataSharingFeature, {}}}, {});
    } else {
      scoped_feature_list_.InitWithFeaturesAndParameters(
          {}, {{data_sharing::features::kDataSharingFeature}});
    }
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(DataSharingServiceFactoryTest, FeatureEnabledUsesRealService) {
  InitService(/*enable_feature=*/true);
  DataSharingService* service =
      DataSharingServiceFactory::GetForProfile(profile_.get());
  EXPECT_FALSE(service->IsEmptyService());
}

TEST_F(DataSharingServiceFactoryTest, FeatureDisabledUsesEmptyService) {
  InitService(/*enable_feature=*/false);
  DataSharingService* service =
      DataSharingServiceFactory::GetForProfile(profile_.get());
  EXPECT_TRUE(service->IsEmptyService());
}

TEST_F(DataSharingServiceFactoryTest,
       FeatureEnabledUsesEmptyServiceInIncognito) {
  InitService(/*enable_feature=*/true);
  Profile* otr_profile = profile_.get()->GetOffTheRecordProfile(
      Profile::OTRProfileID::PrimaryID(), /*create_if_needed=*/true);
  DataSharingService* service =
      DataSharingServiceFactory::GetForProfile(otr_profile);
  EXPECT_TRUE(service->IsEmptyService());
}

}  // namespace
}  // namespace data_sharing
