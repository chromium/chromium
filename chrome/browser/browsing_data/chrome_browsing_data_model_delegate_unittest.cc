// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/chrome_browsing_data_model_delegate.h"
#include "base/test/bind.h"
#include "chrome/browser/browsing_topics/browsing_topics_service_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/browsing_topics/test_util.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

class ChromeBrowsingDataModelDelegateTest : public testing::Test {
 public:
  ChromeBrowsingDataModelDelegateTest() = default;

  ChromeBrowsingDataModelDelegateTest(
      const ChromeBrowsingDataModelDelegateTest&) = delete;
  ChromeBrowsingDataModelDelegateTest& operator=(
      const ChromeBrowsingDataModelDelegateTest&) = delete;

  ~ChromeBrowsingDataModelDelegateTest() override = default;

  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    browsing_topics::BrowsingTopicsServiceFactory::GetInstance()
        ->SetTestingFactoryAndUse(
            profile(),
            base::BindLambdaForTesting([this](content::BrowserContext* context)
                                           -> std::unique_ptr<KeyedService> {
              auto mock_browsing_topics_service = std::make_unique<
                  browsing_topics::MockBrowsingTopicsService>();
              mock_browsing_topics_service_ =
                  mock_browsing_topics_service.get();
              return mock_browsing_topics_service;
            }));
  }

  void TearDown() override { profile_.reset(); }

  TestingProfile* profile() { return profile_.get(); }

  browsing_topics::MockBrowsingTopicsService* mock_browsing_topics_service() {
    return mock_browsing_topics_service_;
  }

 protected:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  raw_ptr<browsing_topics::MockBrowsingTopicsService>
      mock_browsing_topics_service_;
  std::unique_ptr<TestingProfile> profile_ = nullptr;
};

TEST_F(ChromeBrowsingDataModelDelegateTest, RemoveDataKeyForTopics) {
  auto testOrigin = url::Origin::Create(GURL("a.test"));
  EXPECT_CALL(*mock_browsing_topics_service(),
              ClearTopicsDataForOrigin(testOrigin))
      .Times(1);
  std::unique_ptr<ChromeBrowsingDataModelDelegate> delegate =
      ChromeBrowsingDataModelDelegate::CreateForProfile(profile());
  EXPECT_TRUE(delegate);

  delegate->RemoveDataKey(
      testOrigin,
      {static_cast<BrowsingDataModel::StorageType>(
          ChromeBrowsingDataModelDelegate::StorageType::kTopics)},
      base::DoNothing());
}
