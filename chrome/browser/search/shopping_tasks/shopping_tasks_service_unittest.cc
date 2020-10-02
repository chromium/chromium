// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/test/bind_test_util.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/search/shopping_tasks/shopping_tasks_service.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class ShoppingTasksServiceTest : public testing::Test {
 public:
  ShoppingTasksServiceTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP) {}

  void SetUp() override {
    testing::Test::SetUp();

    service_ = std::make_unique<ShoppingTasksService>(
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
  std::unique_ptr<ShoppingTasksService> service_;
};

// Verifies correct parsing of well-formed JSON.
TEST_F(ShoppingTasksServiceTest, GoodResponse) {
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
            "info": "visited 5 days ago",
            "target_url": "https://google.com/foo"
          },{
            "name": "bar",
            "image_url": "https://bar.com",
            "price": "$400",
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

  shopping_tasks::mojom::ShoppingTaskPtr result;
  base::MockCallback<ShoppingTasksService::ShoppingTaskCallback> callback;
  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce(testing::Invoke(
          [&result](shopping_tasks::mojom::ShoppingTaskPtr arg) {
            result = std::move(arg);
          }));

  service_->GetPrimaryShoppingTask(callback.Get());
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(result);
  EXPECT_EQ("hello world", result->title);
  EXPECT_EQ(2ul, result->products.size());
  EXPECT_EQ(2ul, result->related_searches.size());
  EXPECT_EQ("foo", result->products[0]->name);
  EXPECT_EQ("https://foo.com/", result->products[0]->image_url.spec());
  EXPECT_EQ("$500", result->products[0]->price);
  EXPECT_EQ("visited 5 days ago", result->products[0]->info);
  EXPECT_EQ("https://google.com/bar", result->products[1]->target_url.spec());
  EXPECT_EQ("bar", result->products[1]->name);
  EXPECT_EQ("https://bar.com/", result->products[1]->image_url.spec());
  EXPECT_EQ("$400", result->products[1]->price);
  EXPECT_EQ("visited 1 day ago", result->products[1]->info);
  EXPECT_EQ("https://google.com/bar", result->products[1]->target_url.spec());
  EXPECT_EQ("baz", result->related_searches[0]->text);
  EXPECT_EQ("https://google.com/baz",
            result->related_searches[0]->target_url.spec());
  EXPECT_EQ("blub", result->related_searches[1]->text);
  EXPECT_EQ("https://google.com/blub",
            result->related_searches[1]->target_url.spec());
}

// Verifies service can handle multiple in flight requests.
TEST_F(ShoppingTasksServiceTest, MultiRequest) {
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

  shopping_tasks::mojom::ShoppingTaskPtr result1;
  base::MockCallback<ShoppingTasksService::ShoppingTaskCallback> callback1;
  EXPECT_CALL(callback1, Run(testing::_))
      .Times(1)
      .WillOnce(testing::Invoke(
          [&result1](shopping_tasks::mojom::ShoppingTaskPtr arg) {
            result1 = std::move(arg);
          }));
  service_->GetPrimaryShoppingTask(callback1.Get());

  shopping_tasks::mojom::ShoppingTaskPtr result2;
  base::MockCallback<ShoppingTasksService::ShoppingTaskCallback> callback2;
  EXPECT_CALL(callback2, Run(testing::_))
      .Times(1)
      .WillOnce(testing::Invoke(
          [&result2](shopping_tasks::mojom::ShoppingTaskPtr arg) {
            result2 = std::move(arg);
          }));
  service_->GetPrimaryShoppingTask(callback2.Get());

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(result1);
  EXPECT_TRUE(result2);
}

// Verifies error if JSON is malformed.
TEST_F(ShoppingTasksServiceTest, BadResponse) {
  test_url_loader_factory_.AddResponse(
      "https://www.google.com/async/newtab_shopping_tasks?hl=en-US",
      ")]}'{\"update\":{\"promotions\":{}}}");

  shopping_tasks::mojom::ShoppingTaskPtr result;
  base::MockCallback<ShoppingTasksService::ShoppingTaskCallback> callback;
  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce(testing::Invoke(
          [&result](shopping_tasks::mojom::ShoppingTaskPtr arg) {
            result = std::move(arg);
          }));

  service_->GetPrimaryShoppingTask(callback.Get());
  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(result);
}

// Verifies error if no products.
TEST_F(ShoppingTasksServiceTest, NoProducts) {
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

  shopping_tasks::mojom::ShoppingTaskPtr result;
  base::MockCallback<ShoppingTasksService::ShoppingTaskCallback> callback;
  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce(testing::Invoke(
          [&result](shopping_tasks::mojom::ShoppingTaskPtr arg) {
            result = std::move(arg);
          }));

  service_->GetPrimaryShoppingTask(callback.Get());
  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(result);
}

// Verifies error if download fails.
TEST_F(ShoppingTasksServiceTest, ErrorResponse) {
  test_url_loader_factory_.AddResponse(
      GURL("https://www.google.com/async/newtab_shopping_tasks?hl=en-US"),
      network::mojom::URLResponseHead::New(), std::string(),
      network::URLLoaderCompletionStatus(net::HTTP_NOT_FOUND));

  shopping_tasks::mojom::ShoppingTaskPtr result;
  base::MockCallback<ShoppingTasksService::ShoppingTaskCallback> callback;
  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce(testing::Invoke(
          [&result](shopping_tasks::mojom::ShoppingTaskPtr arg) {
            result = std::move(arg);
          }));

  service_->GetPrimaryShoppingTask(callback.Get());
  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(result);
}

// Verifies shopping tasks can be dismissed and restored and that the service
// remembers not to return dismissed tasks.
TEST_F(ShoppingTasksServiceTest, DismissTasks) {
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
        "products": [
          {
            "name": "foo",
            "image_url": "https://foo.com",
            "price": "$500",
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

  shopping_tasks::mojom::ShoppingTaskPtr result1;
  base::MockCallback<ShoppingTasksService::ShoppingTaskCallback> callback1;
  EXPECT_CALL(callback1, Run(testing::_))
      .Times(1)
      .WillOnce(testing::Invoke(
          [&result1](shopping_tasks::mojom::ShoppingTaskPtr arg) {
            result1 = std::move(arg);
          }));
  service_->GetPrimaryShoppingTask(callback1.Get());
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(result1);
  EXPECT_EQ("task 1 name", result1->name);

  service_->DismissShoppingTask("task 1 name");

  shopping_tasks::mojom::ShoppingTaskPtr result2;
  base::MockCallback<ShoppingTasksService::ShoppingTaskCallback> callback2;
  EXPECT_CALL(callback2, Run(testing::_))
      .Times(1)
      .WillOnce(testing::Invoke(
          [&result2](shopping_tasks::mojom::ShoppingTaskPtr arg) {
            result2 = std::move(arg);
          }));
  service_->GetPrimaryShoppingTask(callback2.Get());
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(result2);
  EXPECT_EQ("task 2 name", result2->name);

  service_->DismissShoppingTask("task 2 name");

  shopping_tasks::mojom::ShoppingTaskPtr result3;
  base::MockCallback<ShoppingTasksService::ShoppingTaskCallback> callback3;
  EXPECT_CALL(callback3, Run(testing::_))
      .Times(1)
      .WillOnce(testing::Invoke(
          [&result3](shopping_tasks::mojom::ShoppingTaskPtr arg) {
            result3 = std::move(arg);
          }));
  service_->GetPrimaryShoppingTask(callback3.Get());
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(result3);

  service_->RestoreShoppingTask("task 2 name");

  shopping_tasks::mojom::ShoppingTaskPtr result4;
  base::MockCallback<ShoppingTasksService::ShoppingTaskCallback> callback4;
  EXPECT_CALL(callback4, Run(testing::_))
      .Times(1)
      .WillOnce(testing::Invoke(
          [&result4](shopping_tasks::mojom::ShoppingTaskPtr arg) {
            result4 = std::move(arg);
          }));
  service_->GetPrimaryShoppingTask(callback4.Get());
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(result4);
  EXPECT_EQ("task 2 name", result4->name);

  service_->RestoreShoppingTask("task 1 name");

  shopping_tasks::mojom::ShoppingTaskPtr result5;
  base::MockCallback<ShoppingTasksService::ShoppingTaskCallback> callback5;
  EXPECT_CALL(callback5, Run(testing::_))
      .Times(1)
      .WillOnce(testing::Invoke(
          [&result5](shopping_tasks::mojom::ShoppingTaskPtr arg) {
            result5 = std::move(arg);
          }));
  service_->GetPrimaryShoppingTask(callback5.Get());
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(result5);
  EXPECT_EQ("task 1 name", result5->name);
}
