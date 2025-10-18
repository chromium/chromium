// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_service_factory.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_profile.h"
#include "components/contextual_tasks/public/contextual_tasks_service.h"
#include "components/contextual_tasks/public/features.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace contextual_tasks {

class ContextualTasksServiceFactoryTest : public testing::Test {
 protected:
  ContextualTasksServiceFactoryTest() = default;
  ~ContextualTasksServiceFactoryTest() override = default;

 protected:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(ContextualTasksServiceFactoryTest, UsesRealService) {
  feature_list_.InitAndEnableFeature(kContextualTasks);
  std::unique_ptr<TestingProfile> profile = TestingProfile::Builder().Build();

  ContextualTasksService* service =
      ContextualTasksServiceFactory::GetForProfile(profile.get());
  EXPECT_NE(nullptr, service);
}

TEST_F(ContextualTasksServiceFactoryTest, ReturnsNullIfFeatureDisabled) {
  feature_list_.InitAndDisableFeature(kContextualTasks);
  std::unique_ptr<TestingProfile> profile = TestingProfile::Builder().Build();

  ContextualTasksService* service =
      ContextualTasksServiceFactory::GetForProfile(profile.get());
  EXPECT_EQ(nullptr, service);
}

TEST_F(ContextualTasksServiceFactoryTest, UsesRealServiceInIncognito) {
  feature_list_.InitAndEnableFeature(kContextualTasks);
  std::unique_ptr<TestingProfile> profile = TestingProfile::Builder().Build();

  Profile* otr_profile = profile->GetOffTheRecordProfile(
      Profile::OTRProfileID::PrimaryID(), /*create_if_needed=*/true);
  ContextualTasksService* service =
      ContextualTasksServiceFactory::GetForProfile(otr_profile);
  EXPECT_NE(nullptr, service);
}

}  // namespace contextual_tasks
