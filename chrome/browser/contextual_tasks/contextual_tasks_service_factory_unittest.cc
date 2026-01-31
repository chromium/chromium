// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_service_factory.h"

#include <memory>

#include "base/callback_list.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/data_type_store_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/contextual_tasks/public/contextual_tasks_service.h"
#include "components/contextual_tasks/public/features.h"
#include "components/sync/model/data_type_store_service.h"
#include "components/sync/test/data_type_store_test_util.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class FakeDataTypeStoreService : public syncer::DataTypeStoreService {
 public:
  FakeDataTypeStoreService() {
    data_store_factory_ =
        syncer::DataTypeStoreTestUtil::FactoryForInMemoryStoreForTest();
  }
  MOCK_METHOD(const base::FilePath&, GetSyncDataPath, (), (const, override));

  syncer::RepeatingDataTypeStoreFactory GetStoreFactory() override {
    return data_store_factory_;
  }

  MOCK_METHOD(syncer::RepeatingDataTypeStoreFactory,
              GetStoreFactoryForAccountStorage,
              (),
              (override));

  MOCK_METHOD(scoped_refptr<base::SequencedTaskRunner>,
              GetBackendTaskRunner,
              (),
              ());

 protected:
  syncer::RepeatingDataTypeStoreFactory data_store_factory_;
};

namespace contextual_tasks {

class ContextualTasksServiceFactoryTest : public testing::Test {
 protected:
  ContextualTasksServiceFactoryTest() = default;
  ~ContextualTasksServiceFactoryTest() override = default;

  void SetUp() override {
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindRepeating(&ContextualTasksServiceFactoryTest::
                                        OnWillCreateBrowserContextKeyedServices,
                                    base::Unretained(this)));
  }

  void OnWillCreateBrowserContextKeyedServices(
      content::BrowserContext* browser_context) {
    DataTypeStoreServiceFactory::GetInstance()->SetTestingFactoryAndUse(
        browser_context,
        base::BindRepeating([](content::BrowserContext* browser_context)
                                -> std::unique_ptr<KeyedService> {
          return std::make_unique<FakeDataTypeStoreService>();
        }));
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  // Used to set up test factories for each browser context.
  base::CallbackListSubscription create_services_subscription_;
};

TEST_F(ContextualTasksServiceFactoryTest, UsesRealService) {
  feature_list_.InitAndEnableFeature(kContextualTasks);
  std::unique_ptr<TestingProfile> profile = TestingProfile::Builder().Build();

  ContextualTasksService* service =
      ContextualTasksServiceFactory::GetForProfile(profile.get());
  EXPECT_NE(nullptr, service);

  ContextualTask task = service->CreateTask();
  EXPECT_TRUE(task.IsEphemeral());
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
  ContextualTask task = service->CreateTask();
  EXPECT_TRUE(task.IsEphemeral());
}

TEST_F(ContextualTasksServiceFactoryTest, UsesRealServiceInGuest) {
  feature_list_.InitAndEnableFeature(kContextualTasks);
  std::unique_ptr<TestingProfile> profile =
      TestingProfile::Builder().SetGuestSession().Build();

  ContextualTasksService* service =
      ContextualTasksServiceFactory::GetForProfile(profile.get());
  EXPECT_NE(nullptr, service);
  ContextualTask task = service->CreateTask();
  EXPECT_TRUE(task.IsEphemeral());
}

}  // namespace contextual_tasks
