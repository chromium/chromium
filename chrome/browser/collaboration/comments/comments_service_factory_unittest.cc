// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/collaboration/comments/comments_service_factory.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/testing_profile.h"
#include "components/collaboration/public/comments/comments_service.h"
#include "components/collaboration/public/features.h"
#include "components/data_sharing/public/features.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace collaboration::comments {

class CommentsServiceFactoryTest : public testing::Test {
 protected:
  CommentsServiceFactoryTest() = default;
  ~CommentsServiceFactoryTest() override = default;

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(CommentsServiceFactoryTest, FeatureEnabledUsesRealService) {
  scoped_feature_list_.InitWithFeatures(
      {features::kCollaborationComments,
       data_sharing::features::kDataSharingFeature},
      {});
  profile_ = TestingProfile::Builder().Build();

  CommentsService* service =
      CommentsServiceFactory::GetForProfile(profile_.get());
  EXPECT_FALSE(service->IsEmptyService());
}

TEST_F(CommentsServiceFactoryTest, FeatureDisabledUsesEmptyService) {
  scoped_feature_list_.InitWithFeatures(
      {data_sharing::features::kDataSharingFeature},
      {features::kCollaborationComments});
  profile_ = TestingProfile::Builder().Build();

  CommentsService* service =
      CommentsServiceFactory::GetForProfile(profile_.get());
  EXPECT_TRUE(service->IsEmptyService());
}

TEST_F(CommentsServiceFactoryTest, FeatureEnabledUsesNullInIncognito) {
  scoped_feature_list_.InitWithFeatures(
      {features::kCollaborationComments,
       data_sharing::features::kDataSharingFeature},
      {});
  profile_ = TestingProfile::Builder().Build();

  Profile* otr_profile = profile_->GetOffTheRecordProfile(
      Profile::OTRProfileID::PrimaryID(), /*create_if_needed=*/true);
  CommentsService* service = CommentsServiceFactory::GetForProfile(otr_profile);
  EXPECT_EQ(nullptr, service);
}

TEST_F(CommentsServiceFactoryTest, DataSharingDisabledUsesEmptyService) {
  scoped_feature_list_.InitWithFeatures(
      {features::kCollaborationComments},
      {data_sharing::features::kDataSharingFeature});
  profile_ = TestingProfile::Builder().Build();

  CommentsService* service =
      CommentsServiceFactory::GetForProfile(profile_.get());
  EXPECT_TRUE(service->IsEmptyService());
}

}  // namespace collaboration::comments
