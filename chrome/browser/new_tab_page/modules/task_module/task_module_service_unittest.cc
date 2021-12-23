// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/hash/hash.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/new_tab_page/modules/task_module/task_module_service.h"
#include "chrome/test/base/testing_profile.h"
#include "components/search/ntp_features.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/url_util.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class TaskModuleServiceTest : public testing::Test {
 public:
  TaskModuleServiceTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP,
                          base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    testing::Test::SetUp();

    service_ = std::make_unique<TaskModuleService>(
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_),
        &profile_, "en-US");
  }

  void TearDown() override {
    service_.reset();
    test_url_loader_factory_.ClearResponses();
  }

 protected:
  // Required to run tests from UI thread.
  content::BrowserTaskEnvironment task_environment_;
  variations::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};
  TestingProfile profile_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  base::HistogramTester histogram_tester_;
  std::unique_ptr<TaskModuleService> service_;
};

// Verifies correct parsing of well-formed JSON.
TEST_F(TaskModuleServiceTest, GoodShoppingResponse) {
  auto fiveMonthsAgoTimestamp =
      (base::Time::Now() - base::Days(165) - base::Time::UnixEpoch())
          .InSeconds();
  auto twoDaysAgoTimestamp =
      (base::Time::Now() - base::Days(2) - base::Time::UnixEpoch()).InSeconds();
  auto nowTimestamp = (base::Time::Now() - base::Time::UnixEpoch()).InSeconds();
  auto inTwoDaysTimestamp =
      (base::Time::Now() + base::Days(2) - base::Time::UnixEpoch()).InSeconds();
  test_url_loader_factory_.AddResponse(
      "https://www.google.com/async/newtab_shopping_tasks?hl=en-US",
      base::StringPrintf(R"()]}'
{
  "update": {
    "shopping_tasks": [
      {
        "title": "hello world",
        "task_name": "hello world",
        "products": [
          {
            "name": "foo",
            "image_url": "https://foo.com",
            "price": "$500",
            "viewed_timestamp": {
              "seconds": %s
            },
            "target_url": "https://google.com/foo"
          },{
            "name": "bar",
            "image_url": "https://bar.com",
            "price": "$400",
            "viewed_timestamp": {
              "seconds": %s
            },
            "target_url": "https://google.com/bar"
          },{
            "name": "baz",
            "image_url": "https://baz.com",
            "price": "$500",
            "viewed_timestamp": {
              "seconds": %s
            },
            "target_url": "https://google.com/baz"
          },{
            "name": "blub",
            "image_url": "https://blub.com",
            "price": "$600",
            "viewed_timestamp": {
              "seconds": %s
            },
            "target_url": "https://google.com/blub"
          }
        ],
        "related_searches": [
          {
            "text": "baz",
            "target_url": "https://google.com/baz"
          },
          {
            "text": "blub",
            "target_url": "https://google.com/blub"
          }
        ]
      }
    ]
  }
})",
                         base::NumberToString(fiveMonthsAgoTimestamp).c_str(),
                         base::NumberToString(twoDaysAgoTimestamp).c_str(),
                         base::NumberToString(nowTimestamp).c_str(),
                         base::NumberToString(inTwoDaysTimestamp).c_str()));

  task_module::mojom::TaskPtr result;
  base::MockCallback<TaskModuleService::TaskModuleCallback> callback;
  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce(testing::Invoke([&result](task_module::mojom::TaskPtr arg) {
        result = std::move(arg);
      }));

  service_->GetPrimaryTask(task_module::mojom::TaskModuleType::kShopping,
                           callback.Get());
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(result);
  EXPECT_EQ("hello world", result->title);
  EXPECT_EQ(4ul, result->task_items.size());
  EXPECT_EQ(2ul, result->related_searches.size());
  EXPECT_EQ("foo", result->task_items[0]->name);
  EXPECT_EQ("https://foo.com/", result->task_items[0]->image_url.spec());
  EXPECT_EQ("$500", result->task_items[0]->price);
  EXPECT_EQ("Viewed 5 months ago", result->task_items[0]->info);
  EXPECT_EQ("https://google.com/foo", result->task_items[0]->target_url.spec());
  EXPECT_EQ("bar", result->task_items[1]->name);
  EXPECT_EQ("https://bar.com/", result->task_items[1]->image_url.spec());
  EXPECT_EQ("$400", result->task_items[1]->price);
  EXPECT_EQ("Viewed 2 days ago", result->task_items[1]->info);
  EXPECT_EQ("https://google.com/bar", result->task_items[1]->target_url.spec());
  EXPECT_EQ("baz", result->related_searches[0]->text);
  EXPECT_EQ("https://google.com/baz",
            result->related_searches[0]->target_url.spec());
  EXPECT_EQ("blub", result->related_searches[1]->text);
  EXPECT_EQ("https://google.com/blub",
            result->related_searches[1]->target_url.spec());
  ASSERT_EQ(1, histogram_tester_.GetBucketCount(
                   "NewTabPage.Modules.DataRequest",
                   base::PersistentHash("shopping_tasks")));
  EXPECT_EQ("Viewed today", result->task_items[2]->info);
  EXPECT_EQ("Viewed today", result->task_items[3]->info);
}

// Verifies service can handle multiple in flight requests.
TEST_F(TaskModuleServiceTest, MultiRequest) {
  test_url_loader_factory_.AddResponse(
      "https://www.google.com/async/newtab_shopping_tasks?hl=en-US",
      R"()]}'
{
  "update": {
    "shopping_tasks": [
      {
        "title": "hello world",
        "task_name": "hello world",
        "products": [
          {
            "name": "foo",
            "image_url": "https://foo.com",
            "price": "$500",
            "viewed_timestamp": {
              "seconds": 123
            },
            "target_url": "https://google.com/foo"
          }
        ],
        "related_searches": [
          {
            "text": "baz",
            "target_url": "https://google.com/baz"
          }
        ]
      }
    ]
  }
})");

  task_module::mojom::TaskPtr result1;
  base::MockCallback<TaskModuleService::TaskModuleCallback> callback1;
  EXPECT_CALL(callback1, Run(testing::_))
      .Times(1)
      .WillOnce(testing::Invoke([&result1](task_module::mojom::TaskPtr arg) {
        result1 = std::move(arg);
      }));
  service_->GetPrimaryTask(task_module::mojom::TaskModuleType::kShopping,
                           callback1.Get());

  task_module::mojom::TaskPtr result2;
  base::MockCallback<TaskModuleService::TaskModuleCallback> callback2;
  EXPECT_CALL(callback2, Run(testing::_))
      .Times(1)
      .WillOnce(testing::Invoke([&result2](task_module::mojom::TaskPtr arg) {
        result2 = std::move(arg);
      }));
  service_->GetPrimaryTask(task_module::mojom::TaskModuleType::kShopping,
                           callback2.Get());

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(result1);
  EXPECT_TRUE(result2);
  ASSERT_EQ(2, histogram_tester_.GetBucketCount(
                   "NewTabPage.Modules.DataRequest",
                   base::PersistentHash("shopping_tasks")));
}

// Verifies error if JSON is malformed.
TEST_F(TaskModuleServiceTest, BadShoppingResponse) {
  test_url_loader_factory_.AddResponse(
      "https://www.google.com/async/newtab_shopping_tasks?hl=en-US",
      ")]}'{\"update\":{\"promotions\":{}}}");

  task_module::mojom::TaskPtr result;
  base::MockCallback<TaskModuleService::TaskModuleCallback> callback;
  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce(testing::Invoke([&result](task_module::mojom::TaskPtr arg) {
        result = std::move(arg);
      }));

  service_->GetPrimaryTask(task_module::mojom::TaskModuleType::kShopping,
                           callback.Get());
  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(result);
  ASSERT_EQ(1, histogram_tester_.GetBucketCount(
                   "NewTabPage.Modules.DataRequest",
                   base::PersistentHash("shopping_tasks")));
}

// Verifies error if no products.
TEST_F(TaskModuleServiceTest, NoTaskItems) {
  test_url_loader_factory_.AddResponse(
      "https://www.google.com/async/newtab_shopping_tasks?hl=en-US",
      R"()]}'
{
  "update": {
    "shopping_tasks": [
      {
        "title": "hello world",
        "task_name": "hello world",
        "products": [],
        "related_searches": [
          {
            "text": "baz",
            "target_url": "https://google.com/baz"
          }
        ]
      }
    ]
  }
})");

  task_module::mojom::TaskPtr result;
  base::MockCallback<TaskModuleService::TaskModuleCallback> callback;
  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce(testing::Invoke([&result](task_module::mojom::TaskPtr arg) {
        result = std::move(arg);
      }));

  service_->GetPrimaryTask(task_module::mojom::TaskModuleType::kShopping,
                           callback.Get());
  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(result);
  ASSERT_EQ(1, histogram_tester_.GetBucketCount(
                   "NewTabPage.Modules.DataRequest",
                   base::PersistentHash("shopping_tasks")));
}

// Verifies error if download fails.
TEST_F(TaskModuleServiceTest, ErrorResponse) {
  test_url_loader_factory_.AddResponse(
      GURL("https://www.google.com/async/newtab_shopping_tasks?hl=en-US"),
      network::mojom::URLResponseHead::New(), std::string(),
      network::URLLoaderCompletionStatus(net::HTTP_NOT_FOUND));

  task_module::mojom::TaskPtr result;
  base::MockCallback<TaskModuleService::TaskModuleCallback> callback;
  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce(testing::Invoke([&result](task_module::mojom::TaskPtr arg) {
        result = std::move(arg);
      }));

  service_->GetPrimaryTask(task_module::mojom::TaskModuleType::kShopping,
                           callback.Get());
  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(result);
  ASSERT_EQ(1, histogram_tester_.GetBucketCount(
                   "NewTabPage.Modules.DataRequest",
                   base::PersistentHash("shopping_tasks")));
}

// Verifies shopping tasks can be dismissed and restored and that the service
// remembers not to return dismissed tasks.
TEST_F(TaskModuleServiceTest, DismissTasks) {
  test_url_loader_factory_.AddResponse(
      "https://www.google.com/async/newtab_shopping_tasks?hl=en-US",
      R"()]}'
{
  "update": {
    "shopping_tasks": [
      {
        "title": "task 1 title",
        "task_name": "task 1 name",
        "products": [
          {
            "name": "foo",
            "image_url": "https://foo.com",
            "price": "$500",
            "viewed_timestamp": {
              "seconds": 123
            },
            "target_url": "https://google.com/foo"
          }
        ],
        "related_searches": [
          {
            "text": "baz",
            "target_url": "https://google.com/baz"
          }
        ]
      },
      {
        "title": "task 2 title",
        "task_name": "task 2 name",
        "products": [
          {
            "name": "foo",
            "image_url": "https://foo.com",
            "price": "$500",
            "viewed_timestamp": {
              "seconds": 123
            },
            "target_url": "https://google.com/foo"
          }
        ],
        "related_searches": [
          {
            "text": "baz",
            "target_url": "https://google.com/baz"
          }
        ]
      }
    ]
  }
})");

  task_module::mojom::TaskPtr result1;
  base::MockCallback<TaskModuleService::TaskModuleCallback> callback1;
  EXPECT_CALL(callback1, Run(testing::_))
      .Times(1)
      .WillOnce(testing::Invoke([&result1](task_module::mojom::TaskPtr arg) {
        result1 = std::move(arg);
      }));
  service_->GetPrimaryTask(task_module::mojom::TaskModuleType::kShopping,
                           callback1.Get());
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(result1);
  EXPECT_EQ("task 1 name", result1->name);

  service_->DismissTask(task_module::mojom::TaskModuleType::kShopping,
                        "task 1 name");

  task_module::mojom::TaskPtr result2;
  base::MockCallback<TaskModuleService::TaskModuleCallback> callback2;
  EXPECT_CALL(callback2, Run(testing::_))
      .Times(1)
      .WillOnce(testing::Invoke([&result2](task_module::mojom::TaskPtr arg) {
        result2 = std::move(arg);
      }));
  service_->GetPrimaryTask(task_module::mojom::TaskModuleType::kShopping,
                           callback2.Get());
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(result2);
  EXPECT_EQ("task 2 name", result2->name);

  service_->DismissTask(task_module::mojom::TaskModuleType::kShopping,
                        "task 2 name");

  task_module::mojom::TaskPtr result3;
  base::MockCallback<TaskModuleService::TaskModuleCallback> callback3;
  EXPECT_CALL(callback3, Run(testing::_))
      .Times(1)
      .WillOnce(testing::Invoke([&result3](task_module::mojom::TaskPtr arg) {
        result3 = std::move(arg);
      }));
  service_->GetPrimaryTask(task_module::mojom::TaskModuleType::kShopping,
                           callback3.Get());
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(result3);

  service_->RestoreTask(task_module::mojom::TaskModuleType::kShopping,
                        "task 2 name");

  task_module::mojom::TaskPtr result4;
  base::MockCallback<TaskModuleService::TaskModuleCallback> callback4;
  EXPECT_CALL(callback4, Run(testing::_))
      .Times(1)
      .WillOnce(testing::Invoke([&result4](task_module::mojom::TaskPtr arg) {
        result4 = std::move(arg);
      }));
  service_->GetPrimaryTask(task_module::mojom::TaskModuleType::kShopping,
                           callback4.Get());
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(result4);
  EXPECT_EQ("task 2 name", result4->name);

  service_->RestoreTask(task_module::mojom::TaskModuleType::kShopping,
                        "task 1 name");

  task_module::mojom::TaskPtr result5;
  base::MockCallback<TaskModuleService::TaskModuleCallback> callback5;
  EXPECT_CALL(callback5, Run(testing::_))
      .Times(1)
      .WillOnce(testing::Invoke([&result5](task_module::mojom::TaskPtr arg) {
        result5 = std::move(arg);
      }));
  service_->GetPrimaryTask(task_module::mojom::TaskModuleType::kShopping,
                           callback5.Get());
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(result5);
  EXPECT_EQ("task 1 name", result5->name);

  ASSERT_EQ(5, histogram_tester_.GetBucketCount(
                   "NewTabPage.Modules.DataRequest",
                   base::PersistentHash("shopping_tasks")));
}

// Verifies caching param is added if requested.
TEST_F(TaskModuleServiceTest, CachingParam) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{ntp_features::kNtpShoppingTasksModule,
        {{ntp_features::kNtpShoppingTasksModuleCacheMaxAgeSParam, "123"}}}},
      {});

  service_->GetPrimaryTask(task_module::mojom::TaskModuleType::kShopping,
                           TaskModuleService::TaskModuleCallback());
  base::RunLoop().RunUntilIdle();

  GURL url = test_url_loader_factory_.pending_requests()->at(0).request.url;
  std::string async_param;
  net::GetValueForKeyInQuery(url, "async", &async_param);
  EXPECT_EQ("cache_max_age_s:123", async_param);
}

// Verifies no caching param is added if not requested.
TEST_F(TaskModuleServiceTest, NoCachingParam) {
  service_->GetPrimaryTask(task_module::mojom::TaskModuleType::kShopping,
                           TaskModuleService::TaskModuleCallback());
  base::RunLoop().RunUntilIdle();

  GURL url = test_url_loader_factory_.pending_requests()->at(0).request.url;
  std::string async_param;
  net::GetValueForKeyInQuery(url, "async", &async_param);
  EXPECT_EQ("", async_param);
}

// Verifies that no data request is logged if load comes from cache.
TEST_F(TaskModuleServiceTest, NoLogIfCached) {
  network::URLLoaderCompletionStatus status;
  status.exists_in_cache = true;
  test_url_loader_factory_.AddResponse(
      GURL("https://www.google.com/async/newtab_shopping_tasks?hl=en-US"),
      network::CreateURLResponseHead(net::HTTP_OK),
      R"()]}'
{
  "update": {
    "shopping_tasks": [
      {
        "title": "hello world",
        "task_name": "hello world",
        "products": [
          {
            "name": "foo",
            "image_url": "https://foo.com",
            "price": "$500",
            "viewed_timestamp": {
              "seconds": 123
            },
            "target_url": "https://google.com/foo"
          }
        ],
        "related_searches": [
          {
            "text": "baz",
            "target_url": "https://google.com/baz"
          }
        ]
      }
    ]
  }
})",
      status);

  bool received_response = false;
  base::MockCallback<TaskModuleService::TaskModuleCallback> callback;
  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce(testing::Invoke(
          [&received_response](task_module::mojom::TaskPtr arg) {
            received_response = static_cast<bool>(arg);
          }));
  service_->GetPrimaryTask(task_module::mojom::TaskModuleType::kShopping,
                           callback.Get());
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(received_response);
  EXPECT_EQ(0, histogram_tester_.GetBucketCount(
                   "NewTabPage.Modules.DataRequest",
                   base::PersistentHash("shopping_tasks")));
}

class TaskModuleServiceModulesRedesignedTest : public TaskModuleServiceTest {
 public:
  TaskModuleServiceModulesRedesignedTest() {
    features_.InitAndEnableFeature(ntp_features::kNtpModulesRedesigned);
  }

 private:
  base::test::ScopedFeatureList features_;
};

// Verifies that dismiss is ignored.
TEST_F(TaskModuleServiceModulesRedesignedTest, IgnoresDismiss) {
  test_url_loader_factory_.AddResponse(
      "https://www.google.com/async/newtab_recipe_tasks?hl=en-US",
      R"()]}'
{
  "update": {
    "recipe_tasks": [
      {
        "title": "task title",
        "task_name": "task name",
        "recipes": [
          {
            "name": "foo",
            "image_url": "https://foo.com",
            "viewed_timestamp": {
              "seconds": 123
            },
            "site_name": "bar",
            "target_url": "https://google.com/foo"
          }
        ],
        "related_searches": [
          {
            "text": "baz",
            "target_url": "https://google.com/baz"
          }
        ]
      }
    ]
  }
})");

  bool passed_data = false;
  base::MockCallback<TaskModuleService::TaskModuleCallback> callback;
  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce(
          testing::Invoke([&passed_data](task_module::mojom::TaskPtr arg) {
            passed_data = (arg.get() != nullptr);
          }));

  service_->DismissTask(task_module::mojom::TaskModuleType::kRecipe,
                        "task name");
  service_->GetPrimaryTask(task_module::mojom::TaskModuleType::kRecipe,
                           callback.Get());
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(passed_data);
}
