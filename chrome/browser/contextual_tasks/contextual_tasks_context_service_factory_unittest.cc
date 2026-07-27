// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_context_service_factory.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_context_service.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/contextual_tasks/public/features.h"
#include "components/passage_embeddings/core/passage_embeddings_features.h"
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
  feature_list_.InitWithFeatures(
      {kContextualTasksContext, passage_embeddings::kPassageEmbedder}, {});
  TestingProfile::Builder builder;
  builder.AddTestingFactory(
      OptimizationGuideKeyedServiceFactory::GetInstance(),
      base::BindRepeating([](content::BrowserContext* context)
                              -> std::unique_ptr<KeyedService> {
        return std::make_unique<
            testing::NiceMock<MockOptimizationGuideKeyedService>>();
      }));
  std::unique_ptr<TestingProfile> profile = builder.Build();
  ContextualTasksContextService* service =
      ContextualTasksContextServiceFactory::GetForProfile(profile.get());
  EXPECT_NE(nullptr, service);
}

// Regression test for a crash: when the OptimizationGuideKeyedService is
// unavailable (e.g. --disable-features=OptimizationHints), the factory must
// not build the service. Otherwise ContextualTasksContextService hands a null
// OptimizationGuideModelProvider to the optimization_guide::ModelHandler
// constructor, which dereferences it and crashes in release builds.
TEST_F(ContextualTasksContextServiceFactoryTest,
       DoesNotCreateServiceWhenOptimizationGuideUnavailable) {
  feature_list_.InitWithFeatures(
      {kContextualTasksContext, passage_embeddings::kPassageEmbedder}, {});
  TestingProfile::Builder builder;
  // No OptimizationGuideKeyedService for this profile.
  builder.AddTestingFactory(
      OptimizationGuideKeyedServiceFactory::GetInstance(),
      base::BindRepeating(
          [](content::BrowserContext* context)
              -> std::unique_ptr<KeyedService> { return nullptr; }));
  std::unique_ptr<TestingProfile> profile = builder.Build();
  ContextualTasksContextService* service =
      ContextualTasksContextServiceFactory::GetForProfile(profile.get());
  EXPECT_EQ(nullptr, service);
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

TEST_F(ContextualTasksContextServiceFactoryTest,
       DoesNotCreateServiceIfFeatureDisabled) {
  feature_list_.InitWithFeatures({passage_embeddings::kPassageEmbedder},
                                 {kContextualTasksContext});
  std::unique_ptr<TestingProfile> profile = TestingProfile::Builder().Build();
  ContextualTasksContextService* service =
      ContextualTasksContextServiceFactory::GetForProfile(profile.get());
  EXPECT_EQ(nullptr, service);
}

TEST_F(ContextualTasksContextServiceFactoryTest,
       DoesNotCreateServiceForIncognito) {
  feature_list_.InitWithFeatures(
      {kContextualTasksContext, passage_embeddings::kPassageEmbedder}, {});
  std::unique_ptr<TestingProfile> profile = TestingProfile::Builder().Build();
  Profile* otr_profile = profile->GetOffTheRecordProfile(
      Profile::OTRProfileID::PrimaryID(), /*create_if_needed=*/true);
  ContextualTasksContextService* service =
      ContextualTasksContextServiceFactory::GetForProfile(otr_profile);
  EXPECT_EQ(nullptr, service);
}

}  // namespace contextual_tasks
