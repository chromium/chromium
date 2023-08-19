// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/barrier_closure.h"
#include "base/hash/hash.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/new_tab_page/modules/recipes/recipes_service.h"
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

class RecipesServiceTest : public testing::Test {
 public:
  RecipesServiceTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP,
                          base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    testing::Test::SetUp();

    service_ = std::make_unique<RecipesService>(
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
  std::unique_ptr<RecipesService> service_;
};

// Verifies correct parsing of well-formed JSON.
TEST_F(RecipesServiceTest, GoodRecipeResponse) {
  auto quit_closure = task_environment_.QuitClosure();
  auto fiveMonthsAgoTimestamp =
      (base::Time::Now() - base::Days(165) - base::Time::UnixEpoch())
          .InSeconds();
  auto twoDaysAgoTimestamp =
      (base::Time::Now() - base::Days(2) - base::Time::UnixEpoch()).InSeconds();
  auto nowTimestamp = (base::Time::Now() - base::Time::UnixEpoch()).InSeconds();
  auto inTwoDaysTimestamp =
      (base::Time::Now() + base::Days(2) - base::Time::UnixEpoch()).InSeconds();
  test_url_loader_factory_.AddResponse(
      "https://www.google.com/async/newtab_recipe_tasks?hl=en-US",
      base::StringPrintf(R"()]}'
{
  "update": {
    "recipe_tasks": [
      {
        "title": "hello world",
        "task_name": "hello world",
        "recipes": [
          {
            "name": "foo",
            "image_url": "https://foo.com",
            "viewed_timestamp": {
              "seconds": %s
            },
            "site_name": "foox",
            "target_url": "https://google.com/foo"
          },
          {
            "name": "bar",
            "image_url": "https://bar.com",
            "viewed_timestamp": {
              "seconds": %s
            },
            "site_name": "barx",
            "target_url": "https://google.com/bar"
          },
          {
            "name": "baz",
            "image_url": "https://baz.com",
            "viewed_timestamp": {
              "seconds": %s
            },
            "site_name": "bax",
            "target_url": "https://google.com/baz"
          },
          {
            "name": "blub",
            "image_url": "https://blub.com",
            "viewed_timestamp": {
              "seconds": %s
            },
            "site_name": "blubx",
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

  recipes::mojom::TaskPtr result;
  base::MockCallback<RecipesService::RecipesCallback> callback;
  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce(testing::Invoke([&](recipes::mojom::TaskPtr arg) {
        result = std::move(arg);
        quit_closure.Run();
      }));

  service_->GetPrimaryTask(callback.Get());
  task_environment_.RunUntilQuit();

  ASSERT_TRUE(result);
  EXPECT_EQ("hello world", result->title);
  EXPECT_EQ(4ul, result->recipes.size());
  EXPECT_EQ(2ul, result->related_searches.size());
  EXPECT_EQ("foo", result->recipes[0]->name);
  EXPECT_EQ("https://foo.com/", result->recipes[0]->image_url.spec());
  EXPECT_EQ("Viewed previously", result->recipes[0]->info);
  EXPECT_EQ("foox", result->recipes[0]->site_name);
  EXPECT_EQ("https://google.com/foo", result->recipes[0]->target_url.spec());
  EXPECT_EQ("bar", result->recipes[1]->name);
  EXPECT_EQ("https://bar.com/", result->recipes[1]->image_url.spec());
  EXPECT_EQ("Viewed in the past week", result->recipes[1]->info);
  EXPECT_EQ("barx", result->recipes[1]->site_name);
  EXPECT_EQ("https://google.com/bar", result->recipes[1]->target_url.spec());
  EXPECT_EQ("baz", result->related_searches[0]->text);
  EXPECT_EQ("https://google.com/baz",
            result->related_searches[0]->target_url.spec());
  EXPECT_EQ("blub", result->related_searches[1]->text);
  EXPECT_EQ("https://google.com/blub",
            result->related_searches[1]->target_url.spec());
  ASSERT_EQ(1, histogram_tester_.GetBucketCount(
                   "NewTabPage.Modules.DataRequest",
                   base::PersistentHash("recipe_tasks")));
  EXPECT_EQ("Viewed today", result->recipes[2]->info);
  EXPECT_EQ("Viewed today", result->recipes[3]->info);
}

// Verifies service can handle multiple in flight requests.
TEST_F(RecipesServiceTest, MultiRequest) {
  auto quit_closure = task_environment_.QuitClosure();
  auto barrier_closure = base::BarrierClosure(2, quit_closure);

  test_url_loader_factory_.AddResponse(
      "https://www.google.com/async/newtab_recipe_tasks?hl=en-US",
      R"()]}'
{
  "update": {
    "recipe_tasks": [
      {
        "title": "hello world",
        "task_name": "hello world",
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

  recipes::mojom::TaskPtr result1;
  base::MockCallback<RecipesService::RecipesCallback> callback1;
  EXPECT_CALL(callback1, Run(testing::_))
      .Times(1)
      .WillOnce(testing::Invoke([&](recipes::mojom::TaskPtr arg) {
        result1 = std::move(arg);
        barrier_closure.Run();
      }));
  service_->GetPrimaryTask(callback1.Get());

  recipes::mojom::TaskPtr result2;
  base::MockCallback<RecipesService::RecipesCallback> callback2;
  EXPECT_CALL(callback2, Run(testing::_))
      .Times(1)
      .WillOnce(testing::Invoke([&](recipes::mojom::TaskPtr arg) {
        result2 = std::move(arg);
        barrier_closure.Run();
      }));
  service_->GetPrimaryTask(callback2.Get());

  task_environment_.RunUntilQuit();

  EXPECT_TRUE(result1);
  EXPECT_TRUE(result2);
  ASSERT_EQ(2, histogram_tester_.GetBucketCount(
                   "NewTabPage.Modules.DataRequest",
                   base::PersistentHash("recipe_tasks")));
}

// Verifies error if JSON is malformed.
TEST_F(RecipesServiceTest, BadRecipeResponse) {
  auto quit_closure = task_environment_.QuitClosure();
  test_url_loader_factory_.AddResponse(
      "https://www.google.com/async/newtab_recipe_tasks?hl=en-US",
      ")]}'{\"update\":{\"promotions\":{}}}");

  recipes::mojom::TaskPtr result;
  base::MockCallback<RecipesService::RecipesCallback> callback;
  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce(testing::Invoke([&](recipes::mojom::TaskPtr arg) {
        result = std::move(arg);
        quit_closure.Run();
      }));

  service_->GetPrimaryTask(callback.Get());
  task_environment_.RunUntilQuit();

  ASSERT_FALSE(result);
  ASSERT_EQ(1, histogram_tester_.GetBucketCount(
                   "NewTabPage.Modules.DataRequest",
                   base::PersistentHash("recipe_tasks")));
}

// Verifies error if no products.
TEST_F(RecipesServiceTest, NoRecipes) {
  auto quit_closure = task_environment_.QuitClosure();
  test_url_loader_factory_.AddResponse(
      "https://www.google.com/async/newtab_recipe_tasks?hl=en-US",
      R"()]}'
{
  "update": {
    "recipe_tasks": [
      {
        "title": "hello world",
        "task_name": "hello world",
        "recipes": [],
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

  recipes::mojom::TaskPtr result;
  base::MockCallback<RecipesService::RecipesCallback> callback;
  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce(testing::Invoke([&](recipes::mojom::TaskPtr arg) {
        result = std::move(arg);
        quit_closure.Run();
      }));

  service_->GetPrimaryTask(callback.Get());
  task_environment_.RunUntilQuit();

  ASSERT_FALSE(result);
  ASSERT_EQ(1, histogram_tester_.GetBucketCount(
                   "NewTabPage.Modules.DataRequest",
                   base::PersistentHash("recipe_tasks")));
}

// Verifies error if download fails.
TEST_F(RecipesServiceTest, ErrorResponse) {
  auto quit_closure = task_environment_.QuitClosure();
  test_url_loader_factory_.AddResponse(
      GURL("https://www.google.com/async/newtab_recipe_tasks?hl=en-US"),
      network::mojom::URLResponseHead::New(), std::string(),
      network::URLLoaderCompletionStatus(net::HTTP_NOT_FOUND));

  recipes::mojom::TaskPtr result;
  base::MockCallback<RecipesService::RecipesCallback> callback;
  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce(testing::Invoke([&](recipes::mojom::TaskPtr arg) {
        result = std::move(arg);
        quit_closure.Run();
      }));

  service_->GetPrimaryTask(callback.Get());
  task_environment_.RunUntilQuit();

  ASSERT_FALSE(result);
  ASSERT_EQ(1, histogram_tester_.GetBucketCount(
                   "NewTabPage.Modules.DataRequest",
                   base::PersistentHash("recipe_tasks")));
}

// Verifies recipe tasks can be dismissed and restored and that the service
// remembers not to return dismissed tasks.
TEST_F(RecipesServiceTest, DismissTasks) {
  auto quit_closure = task_environment_.QuitClosure();
  test_url_loader_factory_.AddResponse(
      "https://www.google.com/async/newtab_recipe_tasks?hl=en-US",
      R"()]}'
{
  "update": {
    "recipe_tasks": [
      {
        "title": "task 1 title",
        "task_name": "task 1 name",
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
      },
      {
        "title": "task 2 title",
        "task_name": "task 2 name",
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

  recipes::mojom::TaskPtr result1;
  base::MockCallback<RecipesService::RecipesCallback> callback1;
  EXPECT_CALL(callback1, Run(testing::_))
      .Times(1)
      .WillOnce(testing::Invoke([&](recipes::mojom::TaskPtr arg) {
        result1 = std::move(arg);
        quit_closure.Run();
      }));
  service_->GetPrimaryTask(callback1.Get());
  task_environment_.RunUntilQuit();
  ASSERT_TRUE(result1);
  EXPECT_EQ("task 1 name", result1->name);

  service_->DismissTask("task 1 name");

  quit_closure = task_environment_.QuitClosure();
  recipes::mojom::TaskPtr result2;
  base::MockCallback<RecipesService::RecipesCallback> callback2;
  EXPECT_CALL(callback2, Run(testing::_))
      .Times(1)
      .WillOnce(testing::Invoke([&](recipes::mojom::TaskPtr arg) {
        result2 = std::move(arg);
        quit_closure.Run();
      }));
  service_->GetPrimaryTask(callback2.Get());
  task_environment_.RunUntilQuit();
  ASSERT_TRUE(result2);
  EXPECT_EQ("task 2 name", result2->name);

  service_->DismissTask("task 2 name");

  quit_closure = task_environment_.QuitClosure();
  recipes::mojom::TaskPtr result3;
  base::MockCallback<RecipesService::RecipesCallback> callback3;
  EXPECT_CALL(callback3, Run(testing::_))
      .Times(1)
      .WillOnce(testing::Invoke([&](recipes::mojom::TaskPtr arg) {
        result3 = std::move(arg);
        quit_closure.Run();
      }));
  service_->GetPrimaryTask(callback3.Get());
  task_environment_.RunUntilQuit();
  ASSERT_FALSE(result3);

  service_->RestoreTask("task 2 name");

  quit_closure = task_environment_.QuitClosure();
  recipes::mojom::TaskPtr result4;
  base::MockCallback<RecipesService::RecipesCallback> callback4;
  EXPECT_CALL(callback4, Run(testing::_))
      .Times(1)
      .WillOnce(testing::Invoke([&](recipes::mojom::TaskPtr arg) {
        result4 = std::move(arg);
        quit_closure.Run();
      }));
  service_->GetPrimaryTask(callback4.Get());
  task_environment_.RunUntilQuit();
  ASSERT_TRUE(result4);
  EXPECT_EQ("task 2 name", result4->name);

  service_->RestoreTask("task 1 name");

  quit_closure = task_environment_.QuitClosure();
  recipes::mojom::TaskPtr result5;
  base::MockCallback<RecipesService::RecipesCallback> callback5;
  EXPECT_CALL(callback5, Run(testing::_))
      .Times(1)
      .WillOnce(testing::Invoke([&](recipes::mojom::TaskPtr arg) {
        result5 = std::move(arg);
        quit_closure.Run();
      }));
  service_->GetPrimaryTask(callback5.Get());
  task_environment_.RunUntilQuit();
  ASSERT_TRUE(result5);
  EXPECT_EQ("task 1 name", result5->name);

  ASSERT_EQ(5, histogram_tester_.GetBucketCount(
                   "NewTabPage.Modules.DataRequest",
                   base::PersistentHash("recipe_tasks")));
}

// Verifies caching param is added if requested.
TEST_F(RecipesServiceTest, CachingParam) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{ntp_features::kNtpRecipeTasksModule,
        {{ntp_features::kNtpRecipeTasksModuleCacheMaxAgeSParam, "123"}}}},
      {});

  service_->GetPrimaryTask(RecipesService::RecipesCallback());
  base::RunLoop().RunUntilIdle();

  GURL url = test_url_loader_factory_.pending_requests()->at(0).request.url;
  std::string async_param;
  net::GetValueForKeyInQuery(url, "async", &async_param);
  EXPECT_EQ("cache_max_age_s:123", async_param);
}

// Verifies no caching param is added if not requested.
TEST_F(RecipesServiceTest, NoCachingParam) {
  service_->GetPrimaryTask(RecipesService::RecipesCallback());
  base::RunLoop().RunUntilIdle();

  GURL url = test_url_loader_factory_.pending_requests()->at(0).request.url;
  std::string async_param;
  net::GetValueForKeyInQuery(url, "async", &async_param);
  EXPECT_EQ("", async_param);
}

// Verifies experiment group param is added if requested.
TEST_F(RecipesServiceTest, ExperimentGroupParam) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{ntp_features::kNtpRecipeTasksModule,
        {{ntp_features::kNtpRecipeTasksModuleExperimentGroupParam, "foo"}}}},
      {});

  service_->GetPrimaryTask(RecipesService::RecipesCallback());
  base::RunLoop().RunUntilIdle();

  GURL url = test_url_loader_factory_.pending_requests()->at(0).request.url;
  std::string async_param;
  net::GetValueForKeyInQuery(url, "async", &async_param);
  EXPECT_EQ("experiment_group:foo", async_param);
}

// Verifies that no data request is logged if load comes from cache.
TEST_F(RecipesServiceTest, NoLogIfCached) {
  auto quit_closure = task_environment_.QuitClosure();
  network::URLLoaderCompletionStatus status;
  status.exists_in_cache = true;
  test_url_loader_factory_.AddResponse(
      GURL("https://www.google.com/async/newtab_recipe_tasks?hl=en-US"),
      network::CreateURLResponseHead(net::HTTP_OK),
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
})",
      status);

  bool received_response = false;
  base::MockCallback<RecipesService::RecipesCallback> callback;
  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce(testing::Invoke([&](recipes::mojom::TaskPtr arg) {
        received_response = static_cast<bool>(arg);
        quit_closure.Run();
      }));
  service_->GetPrimaryTask(callback.Get());
  task_environment_.RunUntilQuit();

  EXPECT_TRUE(received_response);
  EXPECT_EQ(0, histogram_tester_.GetBucketCount(
                   "NewTabPage.Modules.DataRequest",
                   base::PersistentHash("recipe_tasks")));
}

class RecipesServiceModulesRedesignedTest : public RecipesServiceTest {
 public:
  RecipesServiceModulesRedesignedTest() {
    features_.InitAndEnableFeature(ntp_features::kNtpModulesRedesigned);
  }

 private:
  base::test::ScopedFeatureList features_;
};

// Verifies that dismiss is ignored.
TEST_F(RecipesServiceModulesRedesignedTest, IgnoresDismiss) {
  auto quit_closure = task_environment_.QuitClosure();
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
  base::MockCallback<RecipesService::RecipesCallback> callback;
  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce(testing::Invoke([&](recipes::mojom::TaskPtr arg) {
        passed_data = (arg.get() != nullptr);
        quit_closure.Run();
      }));

  service_->DismissTask("task name");
  service_->GetPrimaryTask(callback.Get());
  task_environment_.RunUntilQuit();

  ASSERT_TRUE(passed_data);
}
