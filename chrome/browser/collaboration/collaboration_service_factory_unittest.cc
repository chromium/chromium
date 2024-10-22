// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/collaboration/collaboration_service_factory.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/testing_profile.h"
#include "components/collaboration/internal/collaboration_service_impl.h"
#include "components/collaboration/internal/empty_collaboration_service.h"
#include "components/data_sharing/public/features.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace collaboration {
namespace {

class CollaborationServiceFactoryTest : public testing::Test {
 protected:
  CollaborationServiceFactoryTest() = default;

  ~CollaborationServiceFactoryTest() override = default;

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

TEST_F(CollaborationServiceFactoryTest, FeatureEnabledUsesRealService) {
  InitService(/*enable_feature=*/true);
  CollaborationService* service =
      CollaborationServiceFactory::GetForProfile(profile_.get());
  EXPECT_FALSE(service->IsEmptyService());
}

TEST_F(CollaborationServiceFactoryTest, FeatureDisabledUsesEmptyService) {
  InitService(/*enable_feature=*/false);
  CollaborationService* service =
      CollaborationServiceFactory::GetForProfile(profile_.get());
  EXPECT_TRUE(service->IsEmptyService());
}

TEST_F(CollaborationServiceFactoryTest,
       FeatureEnabledUsesEmptyServiceInIncognito) {
  InitService(/*enable_feature=*/true);
  Profile* otr_profile = profile_.get()->GetOffTheRecordProfile(
      Profile::OTRProfileID::PrimaryID(), /*create_if_needed=*/true);
  CollaborationService* service =
      CollaborationServiceFactory::GetForProfile(otr_profile);
  EXPECT_TRUE(service->IsEmptyService());
}
}  // namespace
}  // namespace collaboration
