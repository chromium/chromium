// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/context_hub/context_hub_service_factory.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/context_hub/context_hub_service.h"
#include "chrome/browser/context_hub/features.h"
#include "chrome/browser/context_hub/memory_bank/memory_bank_entry.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/page_content_annotations/page_content_extraction_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/page_content_annotations/content/page_content_extraction_service.h"
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
    ContextHubServiceFactory::GetInstance();
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
    page_content_annotations::PageContentExtractionServiceFactory::GetInstance()
        ->SetTestingFactoryAndUse(
            browser_context,
            base::BindRepeating([](content::BrowserContext* context)
                                    -> std::unique_ptr<KeyedService> {
              return std::make_unique<
                  page_content_annotations::PageContentExtractionService>(
                  /*os_crypt_async=*/nullptr, context->GetPath(),
                  /*tracker=*/nullptr);
            }));
  }

  base::test::ScopedFeatureList optimization_hints_feature_list_{
      optimization_guide::features::kOptimizationHints};
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

  base::test::TestFuture<bool> save_future;
  service->SaveMemoryBankEntry(
      MemoryBankEntry(MemoryBankType::kTab, GURL("https://example.com"),
                      "Title", "Page text"),
      save_future.GetCallback());
  ASSERT_FALSE(save_future.Get());

  base::test::TestFuture<std::vector<MemoryBankEntry>> get_entries_future;
  service->GetAllEntries(get_entries_future.GetCallback());
  EXPECT_TRUE(get_entries_future.Get().empty());
}

TEST_F(ContextHubServiceFactoryTest,
       CreatesServiceWithMemoryBankWhenFlagEnabled) {
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/{features::kContextHub, features::kMemoryBanks},
      /*disabled_features=*/{features::kContextHubDatabaseStorage});
  TestingProfile profile;
  ContextHubService* service =
      ContextHubServiceFactory::GetForProfile(&profile);
  ASSERT_NE(nullptr, service);

  base::test::TestFuture<bool> save_future;
  service->SaveMemoryBankEntry(
      MemoryBankEntry(MemoryBankType::kTab, GURL("https://example.com"),
                      "Title", "Page text"),
      save_future.GetCallback());
  ASSERT_TRUE(save_future.Get());

  base::test::TestFuture<std::vector<MemoryBankEntry>> get_entries_future;
  service->GetAllEntries(get_entries_future.GetCallback());
  auto entries = get_entries_future.Get();
  ASSERT_EQ(entries.size(), 1u);
  EXPECT_EQ(entries[0].tab_title, "Title");
  EXPECT_FALSE(base::PathExists(
      profile.GetPath().Append(FILE_PATH_LITERAL("ContextHub.db"))));
}

TEST_F(ContextHubServiceFactoryTest,
       CreatesServiceWithDatabaseMemoryBankWhenFlagEnabled) {
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/{features::kContextHub, features::kMemoryBanks,
                            features::kContextHubDatabaseStorage},
      /*disabled_features=*/{});
  TestingProfile profile;
  ContextHubService* service =
      ContextHubServiceFactory::GetForProfile(&profile);
  ASSERT_NE(nullptr, service);

  base::test::TestFuture<bool> save_future;
  service->SaveMemoryBankEntry(
      MemoryBankEntry(MemoryBankType::kTab, GURL("https://example.com"),
                      "Title", "Page text"),
      save_future.GetCallback());
  ASSERT_TRUE(save_future.Get());

  base::test::TestFuture<std::vector<MemoryBankEntry>> get_entries_future;
  service->GetAllEntries(get_entries_future.GetCallback());
  auto entries = get_entries_future.Get();
  ASSERT_EQ(entries.size(), 1u);
  EXPECT_EQ(entries[0].tab_title, "Title");
  EXPECT_TRUE(base::PathExists(
      profile.GetPath().Append(FILE_PATH_LITERAL("ContextHub.db"))));
}

TEST_F(ContextHubServiceFactoryTest,
       CreatesDatabaseStorageWhenMemoryBanksDisabled) {
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/{features::kContextHub,
                            features::kContextHubDatabaseStorage},
      /*disabled_features=*/{features::kMemoryBanks});
  TestingProfile profile;
  ContextHubService* service =
      ContextHubServiceFactory::GetForProfile(&profile);
  ASSERT_NE(nullptr, service);

  base::test::TestFuture<bool> save_future;
  service->SaveMemoryBankEntry(
      MemoryBankEntry(MemoryBankType::kTab, GURL("https://example.com"),
                      "Title", "Page text"),
      save_future.GetCallback());
  ASSERT_FALSE(save_future.Get());

  // Memory Banks feature is disabled so NoOpMemoryBank returns empty entries.
  base::test::TestFuture<std::vector<MemoryBankEntry>> get_entries_future;
  service->GetAllEntries(get_entries_future.GetCallback());
  EXPECT_TRUE(get_entries_future.Get().empty());

  // Database backend is not created when memory banks are disabled.
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(base::PathExists(
      profile.GetPath().Append(FILE_PATH_LITERAL("ContextHub.db"))));
}

TEST_F(ContextHubServiceFactoryTest, DeleteDatabaseWhenFeaturesDisabled) {
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/{features::kContextHub, features::kMemoryBanks},
      /*disabled_features=*/{features::kContextHubDatabaseStorage});

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath db_path =
      temp_dir.GetPath().Append(FILE_PATH_LITERAL("ContextHub.db"));

  ASSERT_TRUE(base::WriteFile(db_path, "dummy content"));
  ASSERT_TRUE(base::PathExists(db_path));

  TestingProfile profile(temp_dir.GetPath());

  EXPECT_NE(nullptr, ContextHubServiceFactory::GetForProfile(&profile));

  task_environment_.RunUntilIdle();
  EXPECT_FALSE(base::PathExists(db_path));
}

}  // namespace context_hub
