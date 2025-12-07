// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/data_sharing/personal_collaboration_data/personal_collaboration_data_service_factory.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/testing_profile.h"
#include "components/data_sharing/public/features.h"
#include "components/data_sharing/public/personal_collaboration_data/personal_collaboration_data_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace data_sharing::personal_collaboration_data {

class PersonalCollaborationDataServiceFactoryTest : public testing::Test {
 protected:
  PersonalCollaborationDataServiceFactoryTest() = default;
  ~PersonalCollaborationDataServiceFactoryTest() override = default;

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(PersonalCollaborationDataServiceFactoryTest,
       FeatureEnabledUsesRealService) {
  scoped_feature_list_.InitWithFeatures(
      {features::kDataSharingAccountDataMigration,
       data_sharing::features::kDataSharingFeature},
      {});
  profile_ = TestingProfile::Builder().Build();

  PersonalCollaborationDataService* service =
      PersonalCollaborationDataServiceFactory::GetForProfile(profile_.get());
  EXPECT_TRUE(service);
}

TEST_F(PersonalCollaborationDataServiceFactoryTest,
       FeatureDisabledReturnNullService) {
  scoped_feature_list_.InitWithFeatures(
      {data_sharing::features::kDataSharingFeature},
      {features::kDataSharingAccountDataMigration});
  profile_ = TestingProfile::Builder().Build();

  PersonalCollaborationDataService* service =
      PersonalCollaborationDataServiceFactory::GetForProfile(profile_.get());
  EXPECT_FALSE(service);
}

}  // namespace data_sharing::personal_collaboration_data
