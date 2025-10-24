// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_context_controller_factory.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_context_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_profile.h"
#include "components/contextual_tasks/public/features.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace contextual_tasks {

class ContextualTasksContextControllerFactoryTest : public testing::Test {
 protected:
  ContextualTasksContextControllerFactoryTest() = default;
  ~ContextualTasksContextControllerFactoryTest() override = default;

 protected:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(ContextualTasksContextControllerFactoryTest, CreatesServiceForProfile) {
  feature_list_.InitAndEnableFeature(kContextualTasks);
  std::unique_ptr<TestingProfile> profile = TestingProfile::Builder().Build();
  ContextualTasksContextController* controller =
      ContextualTasksContextControllerFactory::GetForProfile(profile.get());
  EXPECT_NE(nullptr, controller);
}

TEST_F(ContextualTasksContextControllerFactoryTest,
       DoesNotCreateServiceIfFeatureDisabled) {
  feature_list_.InitAndDisableFeature(kContextualTasks);
  std::unique_ptr<TestingProfile> profile = TestingProfile::Builder().Build();
  ContextualTasksContextController* controller =
      ContextualTasksContextControllerFactory::GetForProfile(profile.get());
  EXPECT_EQ(nullptr, controller);
}

TEST_F(ContextualTasksContextControllerFactoryTest,
       CreatesServiceForIncognito) {
  feature_list_.InitAndEnableFeature(kContextualTasks);
  std::unique_ptr<TestingProfile> profile = TestingProfile::Builder().Build();
  Profile* otr_profile = profile->GetOffTheRecordProfile(
      Profile::OTRProfileID::PrimaryID(), /*create_if_needed=*/true);
  ContextualTasksContextController* controller =
      ContextualTasksContextControllerFactory::GetForProfile(otr_profile);
  EXPECT_NE(nullptr, controller);
}

TEST_F(ContextualTasksContextControllerFactoryTest, CreatesServiceForGuest) {
  feature_list_.InitAndEnableFeature(kContextualTasks);
  std::unique_ptr<TestingProfile> profile =
      TestingProfile::Builder().SetGuestSession().Build();
  ContextualTasksContextController* controller =
      ContextualTasksContextControllerFactory::GetForProfile(profile.get());
  EXPECT_NE(nullptr, controller);
}

}  // namespace contextual_tasks
