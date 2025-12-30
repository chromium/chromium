// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_context_service_factory.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_context_service.h"
#include "chrome/test/base/testing_profile.h"
#include "components/contextual_tasks/public/features.h"
#include "components/passage_embeddings/passage_embeddings_features.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace contextual_tasks {

class ContextualTasksContextServiceFactoryTest : public testing::Test {
 protected:
  ContextualTasksContextServiceFactoryTest() = default;
  ~ContextualTasksContextServiceFactoryTest() override = default;

 protected:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
};

// ChromeOS requires a separate flag for the passage embedder, so just skip
// this test on ChromeOS.
#if !BUILDFLAG(IS_CHROMEOS)
TEST_F(ContextualTasksContextServiceFactoryTest, CreatesServiceForProfile) {
  feature_list_.InitAndEnableFeature(kContextualTasksContext);
  std::unique_ptr<TestingProfile> profile = TestingProfile::Builder().Build();
  ContextualTasksContextService* service =
      ContextualTasksContextServiceFactory::GetForProfile(profile.get());
  EXPECT_NE(nullptr, service);
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

TEST_F(ContextualTasksContextServiceFactoryTest,
       DoesNotCreateServiceIfFeatureDisabled) {
  feature_list_.InitAndDisableFeature(kContextualTasksContext);
  std::unique_ptr<TestingProfile> profile = TestingProfile::Builder().Build();
  ContextualTasksContextService* service =
      ContextualTasksContextServiceFactory::GetForProfile(profile.get());
  EXPECT_EQ(nullptr, service);
}

TEST_F(ContextualTasksContextServiceFactoryTest,
       DoesNotCreateServiceForIncognito) {
  feature_list_.InitAndEnableFeature(kContextualTasksContext);
  std::unique_ptr<TestingProfile> profile = TestingProfile::Builder().Build();
  Profile* otr_profile = profile->GetOffTheRecordProfile(
      Profile::OTRProfileID::PrimaryID(), /*create_if_needed=*/true);
  ContextualTasksContextService* service =
      ContextualTasksContextServiceFactory::GetForProfile(otr_profile);
  EXPECT_EQ(nullptr, service);
}

}  // namespace contextual_tasks
