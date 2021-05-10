// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cart/fetch_discount_worker.h"
#include "chrome/browser/cart/cart_discount_fetcher.h"
#include "chrome/browser/endpoint_fetcher/endpoint_fetcher.h"
#include "content/public/test/browser_task_environment.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class MockCartDiscountFetcher : public CartDiscountFetcher {
 public:
  MOCK_METHOD(
      void,
      Fetch,
      (std::unique_ptr<network::PendingSharedURLLoaderFactory> pending_factory,
       CartDiscountFetcherCallback callback),
      (override));
};

class FakeCartDiscountFetcherFactory : public CartDiscountFetcherFactory {
 public:
  explicit FakeCartDiscountFetcherFactory(
      std::unique_ptr<CartDiscountFetcher> fakeFetcher)
      : fake_fetcher_(std::move(fakeFetcher)) {}

  std::unique_ptr<CartDiscountFetcher> createFetcher() override {
    return std::move(fake_fetcher_);
  }

 private:
  std::unique_ptr<CartDiscountFetcher> fake_fetcher_;
};

class FetchDiscountWorkerTest : public testing::Test {
 public:
  void SetUp() override {
    test_shared_url_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);

    std::unique_ptr<MockCartDiscountFetcher> mock_fetcher =
        std::make_unique<MockCartDiscountFetcher>();

    // The EXPECT_CALL has to call here, since the ownership is transferred at
    // the following line.
    EXPECT_CALL(*mock_fetcher, Fetch);

    std::unique_ptr<CartDiscountFetcherFactory>
        fake_cart_discount_fetcher_factory =
            std::make_unique<FakeCartDiscountFetcherFactory>(
                std::move(mock_fetcher));

    fetch_discount_worker_ = std::make_unique<FetchDiscountWorker>(
        std::move(test_shared_url_loader_factory_),
        std::move(fake_cart_discount_fetcher_factory));
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;

  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory>
      test_shared_url_loader_factory_;
  std::unique_ptr<FetchDiscountWorker> fetch_discount_worker_;
};

TEST_F(FetchDiscountWorkerTest, TestStart) {
  fetch_discount_worker_->Start();
  task_environment_.RunUntilIdle();
}
