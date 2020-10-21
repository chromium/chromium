// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/test/bind_test_util.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/search/recipe_tasks/recipe_tasks_service.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class RecipeTasksServiceTest : public testing::Test {
 public:
  RecipeTasksServiceTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP) {}

  void SetUp() override {
    testing::Test::SetUp();

    service_ = std::make_unique<RecipeTasksService>(
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
  TestingProfile profile_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  std::unique_ptr<RecipeTasksService> service_;
};

// Verifies correct parsing of well-formed JSON.
TEST_F(RecipeTasksServiceTest, GoodResponse) {
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
            "info": "visited 5 days ago",
            "target_url": "https://google.com/foo"
          },{
            "name": "bar",
            "image_url": "https://bar.com",
            "info": "visited 1 day ago",
            "target_url": "https://google.com/bar"
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
})");

  recipe_tasks::mojom::RecipeTaskPtr result;
  base::MockCallback<RecipeTasksService::RecipeTaskCallback> callback;
  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce(
          testing::Invoke([&result](recipe_tasks::mojom::RecipeTaskPtr arg) {
            result = std::move(arg);
          }));

  service_->GetPrimaryRecipeTask(callback.Get());
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(result);
  EXPECT_EQ("hello world", result->title);
  EXPECT_EQ(2ul, result->recipes.size());
  EXPECT_EQ(2ul, result->related_searches.size());
  EXPECT_EQ("foo", result->recipes[0]->name);
  EXPECT_EQ("https://foo.com/", result->recipes[0]->image_url.spec());
  EXPECT_EQ("visited 5 days ago", result->recipes[0]->info);
  EXPECT_EQ("https://google.com/bar", result->recipes[1]->target_url.spec());
  EXPECT_EQ("bar", result->recipes[1]->name);
  EXPECT_EQ("https://bar.com/", result->recipes[1]->image_url.spec());
  EXPECT_EQ("visited 1 day ago", result->recipes[1]->info);
  EXPECT_EQ("https://google.com/bar", result->recipes[1]->target_url.spec());
  EXPECT_EQ("baz", result->related_searches[0]->text);
  EXPECT_EQ("https://google.com/baz",
            result->related_searches[0]->target_url.spec());
  EXPECT_EQ("blub", result->related_searches[1]->text);
  EXPECT_EQ("https://google.com/blub",
            result->related_searches[1]->target_url.spec());
}

// Verifies service can handle multiple in flight requests.
TEST_F(RecipeTasksServiceTest, MultiRequest) {
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
            "info": "visited 5 days ago",
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

  recipe_tasks::mojom::RecipeTaskPtr result1;
  base::MockCallback<RecipeTasksService::RecipeTaskCallback> callback1;
  EXPECT_CALL(callback1, Run(testing::_))
      .Times(1)
      .WillOnce(
          testing::Invoke([&result1](recipe_tasks::mojom::RecipeTaskPtr arg) {
            result1 = std::move(arg);
          }));
  service_->GetPrimaryRecipeTask(callback1.Get());

  recipe_tasks::mojom::RecipeTaskPtr result2;
  base::MockCallback<RecipeTasksService::RecipeTaskCallback> callback2;
  EXPECT_CALL(callback2, Run(testing::_))
      .Times(1)
      .WillOnce(
          testing::Invoke([&result2](recipe_tasks::mojom::RecipeTaskPtr arg) {
            result2 = std::move(arg);
          }));
  service_->GetPrimaryRecipeTask(callback2.Get());

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(result1);
  EXPECT_TRUE(result2);
}

// Verifies error if JSON is malformed.
TEST_F(RecipeTasksServiceTest, BadResponse) {
  test_url_loader_factory_.AddResponse(
      "https://www.google.com/async/newtab_recipe_tasks?hl=en-US",
      ")]}'{\"update\":{\"promotions\":{}}}");

  recipe_tasks::mojom::RecipeTaskPtr result;
  base::MockCallback<RecipeTasksService::RecipeTaskCallback> callback;
  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce(
          testing::Invoke([&result](recipe_tasks::mojom::RecipeTaskPtr arg) {
            result = std::move(arg);
          }));

  service_->GetPrimaryRecipeTask(callback.Get());
  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(result);
}

// Verifies error if no recipes.
TEST_F(RecipeTasksServiceTest, NoRecipes) {
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

  recipe_tasks::mojom::RecipeTaskPtr result;
  base::MockCallback<RecipeTasksService::RecipeTaskCallback> callback;
  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce(
          testing::Invoke([&result](recipe_tasks::mojom::RecipeTaskPtr arg) {
            result = std::move(arg);
          }));

  service_->GetPrimaryRecipeTask(callback.Get());
  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(result);
}

// Verifies error if download fails.
TEST_F(RecipeTasksServiceTest, ErrorResponse) {
  test_url_loader_factory_.AddResponse(
      GURL("https://www.google.com/async/newtab_recipe_tasks?hl=en-US"),
      network::mojom::URLResponseHead::New(), std::string(),
      network::URLLoaderCompletionStatus(net::HTTP_NOT_FOUND));

  recipe_tasks::mojom::RecipeTaskPtr result;
  base::MockCallback<RecipeTasksService::RecipeTaskCallback> callback;
  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce(
          testing::Invoke([&result](recipe_tasks::mojom::RecipeTaskPtr arg) {
            result = std::move(arg);
          }));

  service_->GetPrimaryRecipeTask(callback.Get());
  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(result);
}

// Verifies recipe tasks can be dismissed and restored and that the service
// remembers not to return dismissed tasks.
TEST_F(RecipeTasksServiceTest, DismissTasks) {
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
            "info": "visited 5 days ago",
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
            "info": "visited 5 days ago",
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

  recipe_tasks::mojom::RecipeTaskPtr result1;
  base::MockCallback<RecipeTasksService::RecipeTaskCallback> callback1;
  EXPECT_CALL(callback1, Run(testing::_))
      .Times(1)
      .WillOnce(
          testing::Invoke([&result1](recipe_tasks::mojom::RecipeTaskPtr arg) {
            result1 = std::move(arg);
          }));
  service_->GetPrimaryRecipeTask(callback1.Get());
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(result1);
  EXPECT_EQ("task 1 name", result1->name);

  service_->DismissRecipeTask("task 1 name");

  recipe_tasks::mojom::RecipeTaskPtr result2;
  base::MockCallback<RecipeTasksService::RecipeTaskCallback> callback2;
  EXPECT_CALL(callback2, Run(testing::_))
      .Times(1)
      .WillOnce(
          testing::Invoke([&result2](recipe_tasks::mojom::RecipeTaskPtr arg) {
            result2 = std::move(arg);
          }));
  service_->GetPrimaryRecipeTask(callback2.Get());
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(result2);
  EXPECT_EQ("task 2 name", result2->name);

  service_->DismissRecipeTask("task 2 name");

  recipe_tasks::mojom::RecipeTaskPtr result3;
  base::MockCallback<RecipeTasksService::RecipeTaskCallback> callback3;
  EXPECT_CALL(callback3, Run(testing::_))
      .Times(1)
      .WillOnce(
          testing::Invoke([&result3](recipe_tasks::mojom::RecipeTaskPtr arg) {
            result3 = std::move(arg);
          }));
  service_->GetPrimaryRecipeTask(callback3.Get());
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(result3);

  service_->RestoreRecipeTask("task 2 name");

  recipe_tasks::mojom::RecipeTaskPtr result4;
  base::MockCallback<RecipeTasksService::RecipeTaskCallback> callback4;
  EXPECT_CALL(callback4, Run(testing::_))
      .Times(1)
      .WillOnce(
          testing::Invoke([&result4](recipe_tasks::mojom::RecipeTaskPtr arg) {
            result4 = std::move(arg);
          }));
  service_->GetPrimaryRecipeTask(callback4.Get());
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(result4);
  EXPECT_EQ("task 2 name", result4->name);

  service_->RestoreRecipeTask("task 1 name");

  recipe_tasks::mojom::RecipeTaskPtr result5;
  base::MockCallback<RecipeTasksService::RecipeTaskCallback> callback5;
  EXPECT_CALL(callback5, Run(testing::_))
      .Times(1)
      .WillOnce(
          testing::Invoke([&result5](recipe_tasks::mojom::RecipeTaskPtr arg) {
            result5 = std::move(arg);
          }));
  service_->GetPrimaryRecipeTask(callback5.Get());
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(result5);
  EXPECT_EQ("task 1 name", result5->name);
}
