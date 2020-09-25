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
        &profile_);
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
      "https://www.google.com/async/newtab_shopping_tasks",
      R"()]}'
{
  "update": {
    "shopping_tasks": [
      {
        "title": "hello world",
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

  base::Optional<ShoppingTasksData> result;
  base::MockCallback<ShoppingTasksService::ShoppingTaskCallback> callback;
  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce(testing::SaveArg<0>(&result));

  service_->GetPrimaryShoppingTask(callback.Get());
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ("hello world", result->title);
  EXPECT_EQ(2ul, result->products.size());
  EXPECT_EQ(2ul, result->related_searches.size());
  EXPECT_EQ("foo", result->products[0].name);
  EXPECT_EQ("https://foo.com/", result->products[0].image_url.spec());
  EXPECT_EQ("$500", result->products[0].price);
  EXPECT_EQ("visited 5 days ago", result->products[0].info);
  EXPECT_EQ("https://google.com/bar", result->products[1].target_url.spec());
  EXPECT_EQ("bar", result->products[1].name);
  EXPECT_EQ("https://bar.com/", result->products[1].image_url.spec());
  EXPECT_EQ("$400", result->products[1].price);
  EXPECT_EQ("visited 1 day ago", result->products[1].info);
  EXPECT_EQ("https://google.com/bar", result->products[1].target_url.spec());
  EXPECT_EQ("baz", result->related_searches[0].text);
  EXPECT_EQ("https://google.com/baz",
            result->related_searches[0].target_url.spec());
  EXPECT_EQ("blub", result->related_searches[1].text);
  EXPECT_EQ("https://google.com/blub",
            result->related_searches[1].target_url.spec());
}

// Verifies service can handle multiple in flight requests.
TEST_F(ShoppingTasksServiceTest, MultiRequest) {
  test_url_loader_factory_.AddResponse(
      "https://www.google.com/async/newtab_shopping_tasks",
      R"()]}'
{
  "update": {
    "shopping_tasks": [
      {
        "title": "hello world",
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

  base::Optional<ShoppingTasksData> result1;
  base::MockCallback<ShoppingTasksService::ShoppingTaskCallback> callback1;
  EXPECT_CALL(callback1, Run(testing::_))
      .Times(1)
      .WillOnce(testing::SaveArg<0>(&result1));
  service_->GetPrimaryShoppingTask(callback1.Get());

  base::Optional<ShoppingTasksData> result2;
  base::MockCallback<ShoppingTasksService::ShoppingTaskCallback> callback2;
  EXPECT_CALL(callback2, Run(testing::_))
      .Times(1)
      .WillOnce(testing::SaveArg<0>(&result2));
  service_->GetPrimaryShoppingTask(callback2.Get());

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(result1.has_value());
  EXPECT_TRUE(result2.has_value());
}

// Verifies error of JSON is malformed.
TEST_F(ShoppingTasksServiceTest, BadResponse) {
  test_url_loader_factory_.AddResponse(
      "https://www.google.com/async/newtab_shopping_tasks",
      ")]}'{\"update\":{\"promotions\":{}}}");

  base::Optional<ShoppingTasksData> result;
  base::MockCallback<ShoppingTasksService::ShoppingTaskCallback> callback;
  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce(testing::SaveArg<0>(&result));

  service_->GetPrimaryShoppingTask(callback.Get());
  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(result.has_value());
}

// Verifies error if download fails.
TEST_F(ShoppingTasksServiceTest, ErrorResponse) {
  test_url_loader_factory_.AddResponse(
      GURL("https://www.google.com/async/newtab_shopping_tasks"),
      network::mojom::URLResponseHead::New(), std::string(),
      network::URLLoaderCompletionStatus(net::HTTP_NOT_FOUND));

  base::Optional<ShoppingTasksData> result;
  base::MockCallback<ShoppingTasksService::ShoppingTaskCallback> callback;
  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce(testing::SaveArg<0>(&result));

  service_->GetPrimaryShoppingTask(callback.Get());
  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(result.has_value());
}
