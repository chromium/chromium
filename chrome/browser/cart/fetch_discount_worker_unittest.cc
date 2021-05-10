// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cart/fetch_discount_worker.h"
#include "chrome/browser/cart/cart_discount_fetcher.h"
#include "chrome/browser/endpoint_fetcher/endpoint_fetcher.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class FakeCartDiscountFetcher : public CartDiscountFetcher {
 public:
  void Fetch(
      std::unique_ptr<network::PendingSharedURLLoaderFactory> pending_factory,
      CartDiscountFetcherCallback callback) override {
    std::move(callback).Run(std::move(fake_result_));
  }

  void SetFakeFetcherResult(CartDiscountFetcher::CartDiscountMap fake_result) {
    fake_result_ = std::move(fake_result);
  }

 private:
  CartDiscountFetcher::CartDiscountMap fake_result_;
};

class MockCartDiscountFetcher : public CartDiscountFetcher {
 public:
  MOCK_METHOD(
      void,
      Fetch,
      (std::unique_ptr<network::PendingSharedURLLoaderFactory> pending_factory,
       CartDiscountFetcherCallback callback),
      (override));

  void DelegateToFake(CartDiscountMap fake_result) {
    fake_cart_discount_fetcher_.SetFakeFetcherResult(std::move(fake_result));

    ON_CALL(*this, Fetch)
        .WillByDefault(
            [this](std::unique_ptr<network::PendingSharedURLLoaderFactory>
                       pending_factory,
                   CartDiscountFetcherCallback callback) {
              return fake_cart_discount_fetcher_.Fetch(
                  std::move(pending_factory), std::move(callback));
            });
  }

 private:
  FakeCartDiscountFetcher fake_cart_discount_fetcher_;
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

class FakeCartLoader : public CartLoader {
 public:
  explicit FakeCartLoader(Profile* profile) : CartLoader(profile) {}

  void LoadAllCarts(CartDB::LoadCallback callback) override {
    std::move(callback).Run(true, std::move(fake_cart_data_));
  }

  void setFakeCartData(std::vector<CartDB::KeyAndValue> proto_pairs) {
    fake_cart_data_ = std::move(proto_pairs);
  }

 private:
  std::vector<CartDB::KeyAndValue> fake_cart_data_;
};

class FakeCartDiscountUpdater : public CartDiscountUpdater {
 public:
  explicit FakeCartDiscountUpdater(Profile* profile)
      : CartDiscountUpdater(profile) {}

  void update() override {}
};

class FakeCartLoaderAndUpdaterFactory : public CartLoaderAndUpdaterFactory {
 public:
  explicit FakeCartLoaderAndUpdaterFactory(Profile* profile)
      : CartLoaderAndUpdaterFactory(profile), profile_(profile) {}

  std::unique_ptr<CartLoader> createCartLoader() override {
    auto fake_loader = std::make_unique<FakeCartLoader>(profile_);
    fake_loader->setFakeCartData(fake_data_);
    return fake_loader;
  }
  std::unique_ptr<CartDiscountUpdater> createCartDiscountUpdater() override {
    auto fake_updater = std::make_unique<FakeCartDiscountUpdater>(profile_);
    return fake_updater;
  }

  void setCartLoaderFakeData(std::vector<CartDB::KeyAndValue> fake_data) {
    fake_data_ = fake_data;
  }

 private:
  Profile* profile_;
  std::vector<CartDB::KeyAndValue> fake_data_;
};

class FetchDiscountWorkerTest : public testing::Test {
 public:
  void SetUp() override {
    test_shared_url_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);

    mock_fetcher_ = std::make_unique<MockCartDiscountFetcher>();

    fake_cart_loader_and_updater_factory_ =
        std::make_unique<FakeCartLoaderAndUpdaterFactory>(&profile_);
  }

  // This method transfers mock_fetcher_ ownership. Set all expectations for
  // mock_fetcher_ before calling this method.
  void CreateCartDiscountFetcherFactory() {
    fake_cart_discount_fetcher_factory_ =
        std::make_unique<FakeCartDiscountFetcherFactory>(
            std::move(mock_fetcher_));
  }

  // This method transfers ownership of the following objects, please set all
  // expectations before calling this method.
  //   * fake_cart_discount_fetcher_factory_
  //   * fake_cart_loader_and_updater_factory_
  void CreateWorker() {
    fetch_discount_worker_ = std::make_unique<FetchDiscountWorker>(
        std::move(test_shared_url_loader_factory_),
        std::move(fake_cart_discount_fetcher_factory_),
        std::move(fake_cart_loader_and_updater_factory_));
  }

  void CreateFakeFetchedResult() {}

 protected:
  content::BrowserTaskEnvironment task_environment_;

  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory>
      test_shared_url_loader_factory_;
  std::unique_ptr<FetchDiscountWorker> fetch_discount_worker_;
  std::unique_ptr<CartDiscountFetcherFactory>
      fake_cart_discount_fetcher_factory_;

  std::unique_ptr<MockCartDiscountFetcher> mock_fetcher_;

  std::unique_ptr<CartLoaderAndUpdaterFactory>
      fake_cart_loader_and_updater_factory_;

  TestingProfile profile_;
};

TEST_F(FetchDiscountWorkerTest, TestStart_FetcherFetched) {
  EXPECT_CALL(*mock_fetcher_, Fetch);

  CreateCartDiscountFetcherFactory();
  CreateWorker();

  fetch_discount_worker_->Start(base::TimeDelta::FromMilliseconds(0));
  task_environment_.RunUntilIdle();
}

TEST_F(FetchDiscountWorkerTest, TestStart_EndToEnd) {
  CartDiscountFetcher::CartDiscountMap fake_result;
  mock_fetcher_->DelegateToFake(std::move(fake_result));

  CreateCartDiscountFetcherFactory();
  CreateWorker();

  fetch_discount_worker_->Start(base::TimeDelta::FromMilliseconds(0));
  task_environment_.RunUntilIdle();
}
