// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/context_hub/context_hub_service_factory.h"

#include "base/functional/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/context_hub/context_hub_service.h"
#include "chrome/browser/context_hub/features.h"
#include "chrome/browser/context_hub/memory_bank/memory_bank_entry.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace context_hub {

class ContextHubServiceFactoryTest : public testing::Test {
 public:
  ContextHubServiceFactoryTest() = default;
  ~ContextHubServiceFactoryTest() override = default;

 protected:
  void SetUp() override {
    testing::Test::SetUp();
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindRepeating(&ContextHubServiceFactoryTest::
                                        OnWillCreateBrowserContextKeyedServices,
                                    base::Unretained(this)));
  }

  void OnWillCreateBrowserContextKeyedServices(
      content::BrowserContext* browser_context) {
    OptimizationGuideKeyedServiceFactory::GetInstance()
        ->SetTestingFactoryAndUse(
            browser_context,
            base::BindRepeating([](content::BrowserContext* context)
                                    -> std::unique_ptr<KeyedService> {
              return std::make_unique<MockOptimizationGuideKeyedService>();
            }));
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  content::BrowserTaskEnvironment task_environment_;
  base::CallbackListSubscription create_services_subscription_;
};

TEST_F(ContextHubServiceFactoryTest, CreatesServiceWithFlagEnabled) {
  scoped_feature_list_.InitAndEnableFeature(features::kContextHub);
  TestingProfile profile;
  EXPECT_NE(nullptr, ContextHubServiceFactory::GetForProfile(&profile));
}

TEST_F(ContextHubServiceFactoryTest, CreatesNoServiceWithFlagDisabled) {
  scoped_feature_list_.InitAndDisableFeature(features::kContextHub);
  TestingProfile profile;
  EXPECT_EQ(nullptr, ContextHubServiceFactory::GetForProfile(&profile));
}

TEST_F(ContextHubServiceFactoryTest,
       DoesNotCreateServiceForIncognitoWithFlagEnabled) {
  scoped_feature_list_.InitAndEnableFeature(features::kContextHub);
  TestingProfile profile;
  Profile* otr_profile = profile.GetOffTheRecordProfile(
      Profile::OTRProfileID::PrimaryID(), /*create_if_needed=*/true);
  EXPECT_EQ(nullptr, ContextHubServiceFactory::GetForProfile(otr_profile));
}

TEST_F(ContextHubServiceFactoryTest,
       CreatesServiceWithoutMemoryBankWhenFlagDisabled) {
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/{features::kContextHub},
      /*disabled_features=*/{features::kMemoryBanks});
  TestingProfile profile;
  ContextHubService* service =
      ContextHubServiceFactory::GetForProfile(&profile);
  ASSERT_NE(nullptr, service);

  base::test::TestFuture<void> save_future;
  service->SaveTab(GURL("https://example.com"), "Title", "Page text",
                   save_future.GetCallback());
  ASSERT_TRUE(save_future.Wait());

  base::test::TestFuture<std::vector<MemoryBankEntry>> get_entries_future;
  service->GetAllEntries(get_entries_future.GetCallback());
  EXPECT_TRUE(get_entries_future.Get().empty());
}

TEST_F(ContextHubServiceFactoryTest,
       CreatesServiceWithMemoryBankWhenFlagEnabled) {
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/{features::kContextHub, features::kMemoryBanks},
      /*disabled_features=*/{});
  TestingProfile profile;
  ContextHubService* service =
      ContextHubServiceFactory::GetForProfile(&profile);
  ASSERT_NE(nullptr, service);

  base::test::TestFuture<void> save_future;
  service->SaveTab(GURL("https://example.com"), "Title", "Page text",
                   save_future.GetCallback());
  ASSERT_TRUE(save_future.Wait());

  base::test::TestFuture<std::vector<MemoryBankEntry>> get_entries_future;
  service->GetAllEntries(get_entries_future.GetCallback());
  auto entries = get_entries_future.Get();
  EXPECT_EQ(entries.size(), 1u);
  EXPECT_EQ(entries[0].tab_title, "Title");
}

}  // namespace context_hub
