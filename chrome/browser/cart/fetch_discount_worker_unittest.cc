// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cart/fetch_discount_worker.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/cart/cart_discount_fetcher.h"
#include "chrome/browser/endpoint_fetcher/endpoint_fetcher.h"
#include "chrome/test/base/testing_profile.h"
#include "components/search/ntp_features.h"
#include "content/public/test/browser_task_environment.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
cart_db::DiscountInfoProto BuildPercentOffDiscountInfoProto(
    const std::string& rule_id,
    const std::string& merchant_rule_id,
    const std::string& raw_merchant_offer_id,
    const int percent_off) {
  cart_db::DiscountInfoProto proto;
  proto.set_rule_id(rule_id);
  proto.set_merchant_rule_id(merchant_rule_id);
  proto.set_percent_off(percent_off);
  proto.set_raw_merchant_offer_id(raw_merchant_offer_id);
  return proto;
}

cart_db::ChromeCartContentProto BuildCartContentProto(const char* domain,
                                                      const char* merchant_url,
                                                      const double timestamp) {
  cart_db::ChromeCartContentProto proto;
  proto.set_key(domain);
  proto.set_merchant_cart_url(merchant_url);
  proto.set_timestamp(timestamp);
  return proto;
}

cart_db::ChromeCartContentProto AddDiscountToProto(
    cart_db::ChromeCartContentProto proto,
    const std::string& merchant_id,
    cart_db::DiscountInfoProto discount_proto) {
  proto.mutable_discount_info()->set_merchant_id(merchant_id);
  (*(proto.mutable_discount_info()->add_discount_info())) = discount_proto;
  return proto;
}

MATCHER_P(EqualsProto, message, "") {
  std::string expected_serialized, actual_serialized;
  message.SerializeToString(&expected_serialized);
  arg.SerializeToString(&actual_serialized);
  return expected_serialized == actual_serialized;
}

const char kMockMerchantA[] = "foo.com";
const char kMockMerchantACartUrl[] = "https://www.foo.com/cart";
const char kMockMerchantAId[] = "123";
const char kMockMerchantARuleId[] = "456";
const char kMockMerchantARawMerchantOfferId[] = "789";
const char kMockMerchantAHighestPercentOff[] = "10\% off";
const int kMockMerchantAPercentOff = 10;
const double kMockMerchantATimestamp = base::Time::Now().ToDoubleT();
const cart_db::ChromeCartContentProto kMockMerchantACartContentProto =
    BuildCartContentProto(kMockMerchantA,
                          kMockMerchantACartUrl,
                          kMockMerchantATimestamp);
const std::vector<cart_db::DiscountInfoProto> kMockMerchantADiscounts = {
    BuildPercentOffDiscountInfoProto(kMockMerchantARuleId,
                                     kMockMerchantARuleId,
                                     kMockMerchantARawMerchantOfferId,
                                     kMockMerchantAPercentOff)};
}  // namespace

class FakeCartDiscountFetcher : public CartDiscountFetcher {
 public:
  void Fetch(
      std::unique_ptr<network::PendingSharedURLLoaderFactory> pending_factory,
      CartDiscountFetcherCallback callback,
      std::vector<CartDB::KeyAndValue> proto_pairs) override {
    FakeCartDiscountFetcher::fetcher_fetch_count_++;
    std::move(callback).Run(fake_result_, is_tester_);
  }

  void SetFakeFetcherResult(CartDiscountFetcher::CartDiscountMap fake_result) {
    fake_result_ = std::move(fake_result);
  }

  static int GetFetchCount() { return fetcher_fetch_count_; }

  static void ClearFetchCount() { fetcher_fetch_count_ = 0; }

 private:
  CartDiscountFetcher::CartDiscountMap fake_result_;
  static int fetcher_fetch_count_;
  bool is_tester_;
};

int FakeCartDiscountFetcher::fetcher_fetch_count_ = 0;

class MockCartDiscountFetcher : public CartDiscountFetcher {
 public:
  MOCK_METHOD(
      void,
      Fetch,
      (std::unique_ptr<network::PendingSharedURLLoaderFactory> pending_factory,
       CartDiscountFetcherCallback callback,
       std::vector<CartDB::KeyAndValue> proto_pairs),
      (override));

  void DelegateToFake(CartDiscountMap fake_result) {
    fake_cart_discount_fetcher_.SetFakeFetcherResult(std::move(fake_result));

    EXPECT_CALL(*this, Fetch)
        .WillRepeatedly(
            [this](std::unique_ptr<network::PendingSharedURLLoaderFactory>
                       pending_factory,
                   CartDiscountFetcherCallback callback,
                   std::vector<CartDB::KeyAndValue> proto_pairs) {
              return fake_cart_discount_fetcher_.Fetch(
                  std::move(pending_factory), std::move(callback),
                  std::move(proto_pairs));
            });
  }

 private:
  FakeCartDiscountFetcher fake_cart_discount_fetcher_;
};

class FakeCartDiscountFetcherFactory : public CartDiscountFetcherFactory {
 public:
  std::unique_ptr<CartDiscountFetcher> createFetcher() override {
    auto fetcher = std::make_unique<MockCartDiscountFetcher>();
    fetcher->DelegateToFake(fetcher_fake_result_);
    return std::move(fetcher);
  }

  void SetFetcherFakeResult(
      CartDiscountFetcher::CartDiscountMap fetcher_fake_result) {
    fetcher_fake_result_ = std::move(fetcher_fake_result);
  }

 private:
  CartDiscountFetcher::CartDiscountMap fetcher_fake_result_;
};

class FakeCartLoader : public CartLoader {
 public:
  explicit FakeCartLoader(Profile* profile) : CartLoader(profile) {}

  void LoadAllCarts(CartDB::LoadCallback callback) override {
    std::move(callback).Run(true, fake_cart_data_);
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

  void SetExpectedData(
      cart_db::ChromeCartContentProto fake_updater_expected_data,
      bool has_discounts,
      std::string& expected_highest_discount_string) {
    expected_update_data_ = fake_updater_expected_data;
    has_discounts_ = has_discounts;
    expected_highest_discount_string_ = expected_highest_discount_string;
  }

  void update(const std::string& cart_url,
              const cart_db::ChromeCartContentProto new_proto,
              const bool is_tester) override {
    // Verify discount_info.
    int new_proto_discount_size =
        new_proto.discount_info().discount_info_size();
    EXPECT_EQ(new_proto_discount_size,
              expected_update_data_.discount_info().discount_info_size());

    EXPECT_EQ(new_proto_discount_size != 0, has_discounts_);

    for (int i = 0; i < new_proto_discount_size; i++) {
      EXPECT_THAT(
          new_proto.discount_info().discount_info(i),
          EqualsProto(expected_update_data_.discount_info().discount_info(i)));
    }

    const std::string& discount_text =
        new_proto.discount_info().discount_text();
    if (has_discounts_) {
      EXPECT_EQ(discount_text, expected_highest_discount_string_);
    } else {
      EXPECT_TRUE(discount_text.empty());
    }
  }

 private:
  cart_db::ChromeCartContentProto expected_update_data_;
  bool has_discounts_;
  std::string expected_highest_discount_string_;
};

class FakeCartLoaderAndUpdaterFactory : public CartLoaderAndUpdaterFactory {
 public:
  explicit FakeCartLoaderAndUpdaterFactory(Profile* profile)
      : CartLoaderAndUpdaterFactory(profile), profile_(profile) {}

  std::unique_ptr<CartLoader> createCartLoader() override {
    auto fake_loader = std::make_unique<FakeCartLoader>(profile_);
    fake_loader->setFakeCartData(fake_loader_data_);
    return fake_loader;
  }
  std::unique_ptr<CartDiscountUpdater> createCartDiscountUpdater() override {
    auto fake_updater = std::make_unique<FakeCartDiscountUpdater>(profile_);
    fake_updater->SetExpectedData(fake_updater_expected_data_,
                                  fake_updater_has_discounts_,
                                  fake_updater_highest_discount_string_);
    return fake_updater;
  }

  void setCartLoaderFakeData(std::vector<CartDB::KeyAndValue> fake_data) {
    fake_loader_data_ = fake_data;
  }

  void setCartDiscountUpdaterExpectedData(
      cart_db::ChromeCartContentProto fake_updater_expected_data,
      bool has_discounts,
      base::StringPiece fake_updater_highest_discount_string = "") {
    fake_updater_expected_data_ = fake_updater_expected_data;
    fake_updater_has_discounts_ = has_discounts;
    fake_updater_highest_discount_string_ =
        std::string(fake_updater_highest_discount_string);
  }

 private:
  Profile* profile_;
  std::vector<CartDB::KeyAndValue> fake_loader_data_;
  cart_db::ChromeCartContentProto fake_updater_expected_data_;
  bool fake_updater_has_discounts_;
  std::string fake_updater_highest_discount_string_;
};

class FetchDiscountWorkerTest : public testing::Test {
 public:
  FetchDiscountWorkerTest() {
    features_.InitAndEnableFeatureWithParameters(
        ntp_features::kNtpChromeCartModule,
        {{ntp_features::kNtpChromeCartModuleAbandonedCartDiscountParam,
          "true"}});
  }
  void SetUp() override {
    test_shared_url_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);

    mock_fetcher_ = std::make_unique<MockCartDiscountFetcher>();

    fake_cart_loader_and_updater_factory_ =
        std::make_unique<FakeCartLoaderAndUpdaterFactory>(&profile_);
  }

  void TearDown() override { FakeCartDiscountFetcher::ClearFetchCount(); }

  // This method transfers mock_fetcher_ ownership. Set all expectations for
  // mock_fetcher_ before calling this method.
  void CreateCartDiscountFetcherFactory(
      CartDiscountFetcher::CartDiscountMap fetcher_fake_result) {
    auto factory = std::make_unique<FakeCartDiscountFetcherFactory>();
    factory->SetFetcherFakeResult(std::move(fetcher_fake_result));
    fake_cart_discount_fetcher_factory_ = std::move(factory);
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
  // This needs to be destroyed after task_environment, so that any tasks on
  // other threads that might check if features are enabled complete first.
  base::test::ScopedFeatureList features_;
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory>
      test_shared_url_loader_factory_;
  std::unique_ptr<FetchDiscountWorker> fetch_discount_worker_;
  std::unique_ptr<CartDiscountFetcherFactory>
      fake_cart_discount_fetcher_factory_;

  std::unique_ptr<MockCartDiscountFetcher> mock_fetcher_;

  std::unique_ptr<FakeCartLoaderAndUpdaterFactory>
      fake_cart_loader_and_updater_factory_;

  TestingProfile profile_;
};


TEST_F(FetchDiscountWorkerTest, TestStart_EndToEnd) {
  CartDiscountFetcher::CartDiscountMap fake_result;
  CreateCartDiscountFetcherFactory(std::move(fake_result));
  CreateWorker();

  fetch_discount_worker_->Start(base::TimeDelta::FromMilliseconds(0));
  task_environment_.RunUntilIdle();
}

TEST_F(FetchDiscountWorkerTest, TestStart_DiscountUpdatedWithDiscount) {
  CartDiscountFetcher::CartDiscountMap fake_result;
  fake_result.emplace(
      kMockMerchantACartUrl,
      MerchantIdAndDiscounts(kMockMerchantAId, kMockMerchantADiscounts,
                             kMockMerchantAHighestPercentOff));
  CreateCartDiscountFetcherFactory(std::move(fake_result));

  CartDB::KeyAndValue mockMerchantACartContentKeyAndProto =
      std::make_pair(kMockMerchantA, kMockMerchantACartContentProto);
  std::vector<CartDB::KeyAndValue> loader_fake_data(
      1, mockMerchantACartContentKeyAndProto);
  fake_cart_loader_and_updater_factory_->setCartLoaderFakeData(
      loader_fake_data);

  cart_db::ChromeCartContentProto cart_content_proto = BuildCartContentProto(
      kMockMerchantA, kMockMerchantACartUrl, kMockMerchantATimestamp);
  cart_db::ChromeCartContentProto updater_expected_data = AddDiscountToProto(
      cart_content_proto, kMockMerchantAId, kMockMerchantADiscounts[0]);
  fake_cart_loader_and_updater_factory_->setCartDiscountUpdaterExpectedData(
      updater_expected_data, true, kMockMerchantAHighestPercentOff);

  CreateWorker();

  fetch_discount_worker_->Start(base::TimeDelta::FromMilliseconds(0));
  task_environment_.RunUntilIdle();
}

TEST_F(FetchDiscountWorkerTest, TestStart_DiscountUpdatedClearDiscount) {
  // No discount available.
  CartDiscountFetcher::CartDiscountMap fake_result;
  CreateCartDiscountFetcherFactory(std::move(fake_result));

  // Loader fake data contatins discount.
  cart_db::ChromeCartContentProto cart_content_proto = BuildCartContentProto(
      kMockMerchantA, kMockMerchantACartUrl, kMockMerchantATimestamp);
  cart_db::ChromeCartContentProto cart_with_discount = AddDiscountToProto(
      cart_content_proto, kMockMerchantAId, kMockMerchantADiscounts[0]);
  CartDB::KeyAndValue mockMerchantACartContentKeyAndProto =
      std::make_pair(kMockMerchantA, cart_with_discount);
  std::vector<CartDB::KeyAndValue> loader_fake_data(
      1, mockMerchantACartContentKeyAndProto);
  fake_cart_loader_and_updater_factory_->setCartLoaderFakeData(
      loader_fake_data);

  // Updater is expected data without discount.
  cart_db::ChromeCartContentProto updater_expected_data = BuildCartContentProto(
      kMockMerchantA, kMockMerchantACartUrl, kMockMerchantATimestamp);
  fake_cart_loader_and_updater_factory_->setCartDiscountUpdaterExpectedData(
      updater_expected_data, false);

  CreateWorker();

  fetch_discount_worker_->Start(base::TimeDelta::FromMilliseconds(0));
  task_environment_.RunUntilIdle();
}

TEST_F(FetchDiscountWorkerTest, TestStart_FetcherRefetched) {
  CartDiscountFetcher::CartDiscountMap fake_result;
  fake_result.emplace(
      kMockMerchantACartUrl,
      MerchantIdAndDiscounts(kMockMerchantAId, kMockMerchantADiscounts,
                             kMockMerchantAHighestPercentOff));
  CreateCartDiscountFetcherFactory(std::move(fake_result));

  CartDB::KeyAndValue mockMerchantACartContentKeyAndProto =
      std::make_pair(kMockMerchantA, kMockMerchantACartContentProto);
  std::vector<CartDB::KeyAndValue> loader_fake_data(
      1, mockMerchantACartContentKeyAndProto);
  fake_cart_loader_and_updater_factory_->setCartLoaderFakeData(
      loader_fake_data);

  cart_db::ChromeCartContentProto cart_content_proto = BuildCartContentProto(
      kMockMerchantA, kMockMerchantACartUrl, kMockMerchantATimestamp);
  cart_db::ChromeCartContentProto updater_expected_data = AddDiscountToProto(
      cart_content_proto, kMockMerchantAId, kMockMerchantADiscounts[0]);
  fake_cart_loader_and_updater_factory_->setCartDiscountUpdaterExpectedData(
      updater_expected_data, true, kMockMerchantAHighestPercentOff);

  CreateWorker();

  fetch_discount_worker_->Start(base::TimeDelta::FromMilliseconds(0));
  task_environment_.FastForwardBy(base::TimeDelta::FromMilliseconds(1));
  EXPECT_EQ(1, FakeCartDiscountFetcher::GetFetchCount());

  task_environment_.FastForwardBy(base::TimeDelta::FromHours(7));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(2, FakeCartDiscountFetcher::GetFetchCount());
}
