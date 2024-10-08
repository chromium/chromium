// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cart/fetch_discount_worker.h"

#include <string_view>

#include "base/containers/flat_map.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/cart/cart_discount_fetcher.h"
#include "chrome/browser/cart/cart_service_factory.h"
#include "chrome/browser/commerce/coupons/coupon_service.h"
#include "chrome/browser/commerce/coupons/coupon_service_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/autofill/core/browser/data_model/autofill_offer_data.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/commerce_heuristics_data.h"
#include "components/endpoint_fetcher/endpoint_fetcher.h"
#include "components/prefs/pref_service.h"
#include "components/search/ntp_features.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/variations/variations.mojom.h"
#include "components/variations/variations_client.h"
#include "content/public/test/browser_task_environment.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
cart_db::RuleDiscountInfoProto BuildPercentOffDiscountInfoProto(
    const std::string& rule_id,
    const std::string& merchant_rule_id,
    const std::string& raw_merchant_offer_id,
    const int percent_off) {
  cart_db::RuleDiscountInfoProto proto;
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

coupon_db::FreeListingCouponInfoProto BuildFreeListingCouponInfoProto(
    const std::string& description,
    const std::string& code,
    const int64_t id,
    const double expiration_time) {
  coupon_db::FreeListingCouponInfoProto proto;
  proto.set_coupon_description(description);
  proto.set_coupon_code(code);
  proto.set_coupon_id(id);
  proto.set_expiry_time(expiration_time);
  return proto;
}

std::unique_ptr<autofill::AutofillOfferData> BuildCouponsMapValueEntry(
    const GURL& cart_url,
    const coupon_db::FreeListingCouponInfoProto& coupon_info) {
  autofill::DisplayStrings ds;
  ds.value_prop_text = coupon_info.coupon_description();
  auto promo_code = coupon_info.coupon_code();
  auto offer_id = coupon_info.coupon_id();
  auto expiry =
      base::Time::FromSecondsSinceUnixEpoch(coupon_info.expiry_time());
  std::vector<GURL> origins;
  origins.emplace_back(cart_url);
  return std::make_unique<autofill::AutofillOfferData>(
      autofill::AutofillOfferData::FreeListingCouponOffer(
          offer_id, expiry, origins, /*offer_details_url=*/GURL(), ds,
          promo_code));
}

cart_db::ChromeCartContentProto AddRBDDiscountToProto(
    cart_db::ChromeCartContentProto proto,
    const std::string& merchant_id,
    cart_db::RuleDiscountInfoProto discount_proto) {
  proto.mutable_discount_info()->set_merchant_id(merchant_id);
  (*(proto.mutable_discount_info()->add_rule_discount_info())) = discount_proto;
  return proto;
}

cart_db::ChromeCartContentProto AddCouponDiscountToProto(
    cart_db::ChromeCartContentProto proto,
    const std::string& merchant_id,
    bool has_coupons) {
  proto.mutable_discount_info()->set_merchant_id(merchant_id);
  proto.mutable_discount_info()->set_has_coupons(has_coupons);
  return proto;
}

MATCHER_P(EqualsProto, message, "") {
  std::string expected_serialized, actual_serialized;
  message.SerializeToString(&expected_serialized);
  arg.SerializeToString(&actual_serialized);
  return expected_serialized == actual_serialized;
}

testing::Matcher<autofill::DisplayStrings> EqualsDisplayStrings(
    const autofill::DisplayStrings& display_strings) {
  return testing::Field("value_prop_text",
                        &autofill::DisplayStrings::value_prop_text,
                        testing::Eq(display_strings.value_prop_text));
}

testing::Matcher<autofill::AutofillOfferData> EqualsAutofillOfferData(
    const autofill::AutofillOfferData& data) {
  return testing::AllOf(
      testing::Property("offer_id", &autofill::AutofillOfferData::GetOfferId,
                        testing::Eq(data.GetOfferId())),
      testing::Property("promo_code",
                        &autofill::AutofillOfferData::GetPromoCode,
                        testing::Eq(data.GetPromoCode())),
      testing::Property("expiry", &autofill::AutofillOfferData::GetExpiry,
                        testing::Eq(data.GetExpiry())),
      testing::Property("display_strings",
                        &autofill::AutofillOfferData::GetDisplayStrings,
                        EqualsDisplayStrings(data.GetDisplayStrings())));
}

const char kMockMerchantA[] = "foo.com";
const char kMockMerchantACartUrl[] = "https://www.foo.com/cart";
const char kMockMerchantAId[] = "123";
const char kMockMerchantARuleId[] = "456";
const char kMockMerchantARawMerchantOfferId[] = "789";
const char kMockMerchantAHighestPercentOff[] = "10\% off";
const int kMockMerchantAPercentOff = 10;
const double kMockMerchantATimestamp =
    base::Time::Now().InSecondsFSinceUnixEpoch();
const cart_db::ChromeCartContentProto kMockMerchantACartContentProto =
    BuildCartContentProto(kMockMerchantA,
                          kMockMerchantACartUrl,
                          kMockMerchantATimestamp);
const std::vector<cart_db::RuleDiscountInfoProto> kMockMerchantADiscounts = {
    BuildPercentOffDiscountInfoProto(kMockMerchantARuleId,
                                     kMockMerchantARuleId,
                                     kMockMerchantARawMerchantOfferId,
                                     kMockMerchantAPercentOff)};
const char kEmail[] = "mock_email@gmail.com";

const std::vector<coupon_db::FreeListingCouponInfoProto>
    kEmptyCouponDiscountList = {};

const char kGlobalHeuristicsJSONData[] = R"###(
      {
        "no_discount_merchant_regex": "nodiscount"
      }
  )###";
}  // namespace

class FakeCartDiscountFetcher : public CartDiscountFetcher {
 public:
  void Fetch(
      std::unique_ptr<network::PendingSharedURLLoaderFactory> pending_factory,
      CartDiscountFetcherCallback callback,
      std::vector<CartDB::KeyAndValue> proto_pairs,
      bool is_oauth_fetch,
      std::string access_token_str,
      std::string fetch_for_locale,
      std::string variation_headers) override {
    FakeCartDiscountFetcher::fetcher_fetch_count_++;
    // Only oauth fetch has a chance to be a tester.
    bool is_tester = is_tester_ && is_oauth_fetch;
    std::move(callback).Run(fake_result_, is_tester);
  }

  void SetFakeFetcherResult(CartDiscountFetcher::CartDiscountMap fake_result) {
    fake_result_ = std::move(fake_result);
  }

  void SetExpectedTester(bool is_tester) { is_tester_ = is_tester; }

  static int GetFetchCount() { return fetcher_fetch_count_; }

  static void ClearFetchCount() { fetcher_fetch_count_ = 0; }

 private:
  CartDiscountFetcher::CartDiscountMap fake_result_;
  static int fetcher_fetch_count_;
  bool is_tester_{false};
};

int FakeCartDiscountFetcher::fetcher_fetch_count_ = 0;

class MockCartDiscountFetcher : public CartDiscountFetcher {
 public:
  MOCK_METHOD(
      void,
      Fetch,
      (std::unique_ptr<network::PendingSharedURLLoaderFactory> pending_factory,
       CartDiscountFetcherCallback callback,
       std::vector<CartDB::KeyAndValue> proto_pairs,
       bool is_oauth_fetch,
       std::string access_token_str,
       std::string fetch_for_locale,
       std::string variation_headers),
      (override));

  void DelegateToFake(CartDiscountMap fake_result, bool is_tester) {
    fake_cart_discount_fetcher_.SetFakeFetcherResult(std::move(fake_result));
    fake_cart_discount_fetcher_.SetExpectedTester(is_tester);

    EXPECT_CALL(*this, Fetch)
        .WillRepeatedly(
            [this](std::unique_ptr<network::PendingSharedURLLoaderFactory>
                       pending_factory,
                   CartDiscountFetcherCallback callback,
                   std::vector<CartDB::KeyAndValue> proto_pairs,
                   bool is_oauth_fetch, std::string access_token_str,
                   std::string fetch_for_locale,
                   std::string variation_headers) {
              return fake_cart_discount_fetcher_.Fetch(
                  std::move(pending_factory), std::move(callback),
                  std::move(proto_pairs), is_oauth_fetch,
                  std::move(access_token_str), std::move(fetch_for_locale),
                  std::move(variation_headers));
            });
  }

 private:
  FakeCartDiscountFetcher fake_cart_discount_fetcher_;
};

class FakeCartDiscountFetcherFactory : public CartDiscountFetcherFactory {
 public:
  std::unique_ptr<CartDiscountFetcher> createFetcher() override {
    auto fetcher = std::make_unique<MockCartDiscountFetcher>();
    fetcher->DelegateToFake(fetcher_fake_result_, is_tester_);
    return std::move(fetcher);
  }

  void SetFetcherFakeResult(
      CartDiscountFetcher::CartDiscountMap fetcher_fake_result) {
    fetcher_fake_result_ = std::move(fetcher_fake_result);
  }

  void SetExpectedTester(bool is_tester) { is_tester_ = is_tester; }

 private:
  CartDiscountFetcher::CartDiscountMap fetcher_fake_result_;
  bool is_tester_{false};
};

class FakeCartDiscountServiceDelegate : public CartDiscountServiceDelegate {
 public:
  explicit FakeCartDiscountServiceDelegate(CartService* cart_service)
      : CartDiscountServiceDelegate(cart_service), expected_tester_(false) {}

  MOCK_METHOD(void,
              UpdateFreeListingCoupons,
              (const CouponService::CouponsMap& coupon_map),
              (override));

  void LoadAllCarts(CartDB::LoadCallback callback) override {
    std::move(callback).Run(true, fake_load_data_);
  }

  void UpdateCart(const std::string& cart_url,
                  const cart_db::ChromeCartContentProto new_proto,
                  const bool is_tester) override {
    // Verify rbd discount_info.
    int new_proto_rule_discount_size =
        new_proto.discount_info().rule_discount_info_size();
    EXPECT_EQ(
        new_proto_rule_discount_size,
        fake_update_expected_data_.discount_info().rule_discount_info_size());

    EXPECT_EQ(new_proto_rule_discount_size != 0,
              fake_update_has_rule_discounts_);

    for (int i = 0; i < new_proto_rule_discount_size; i++) {
      EXPECT_THAT(new_proto.discount_info().rule_discount_info(i),
                  EqualsProto(fake_update_expected_data_.discount_info()
                                  .rule_discount_info(i)));
    }

    // Verify coupond discount_info
    EXPECT_EQ(new_proto.discount_info().has_coupons(),
              fake_update_has_coupon_discounts_);

    const std::string& discount_text =
        new_proto.discount_info().discount_text();
    if (fake_update_has_rule_discounts_ || fake_update_has_coupon_discounts_) {
      EXPECT_EQ(discount_text, fake_update_highest_discount_string_);
    } else {
      EXPECT_TRUE(discount_text.empty());
    }

    EXPECT_EQ(is_tester, expected_tester_);
  }

  void SetCartLoadFakeData(std::vector<CartDB::KeyAndValue> fake_data) {
    fake_load_data_ = fake_data;
  }

  void SetCartDiscountUpdateExpectedData(
      cart_db::ChromeCartContentProto fake_updater_expected_data,
      bool has_rule_discounts,
      std::string_view fake_updater_highest_discount_string = "",
      bool has_coupon_discounts = false) {
    fake_update_expected_data_ = fake_updater_expected_data;
    fake_update_has_rule_discounts_ = has_rule_discounts;
    fake_update_highest_discount_string_ =
        std::string(fake_updater_highest_discount_string);
    fake_update_has_coupon_discounts_ = has_coupon_discounts;
  }

  void SetExpectedTester(bool is_tester) { expected_tester_ = is_tester; }

  void SetExpectedCouponMap(CouponService::CouponsMap map) {
    expected_coupon_map_ = std::move(map);
    EXPECT_CALL(*this, UpdateFreeListingCoupons)
        .Times(1)
        .WillOnce([this](const CouponService::CouponsMap& coupon_map) {
          UpdateFreeListingCoupons_(coupon_map);
        });
  }

 private:
  std::vector<CartDB::KeyAndValue> fake_load_data_;
  cart_db::ChromeCartContentProto fake_update_expected_data_;
  bool fake_update_has_rule_discounts_ = false;
  bool fake_update_has_coupon_discounts_ = false;
  std::string fake_update_highest_discount_string_;
  bool expected_tester_ = false;
  CouponService::CouponsMap expected_coupon_map_;

  void UpdateFreeListingCoupons_(const CouponService::CouponsMap& coupon_map) {
    EXPECT_EQ(expected_coupon_map_.size(), coupon_map.size());

    for (const auto& expected_entry : expected_coupon_map_) {
      EXPECT_TRUE(coupon_map.count(expected_entry.first));
      const auto& coupon_infos = coupon_map.at(expected_entry.first);
      const auto& expected_coupon_info = expected_entry.second;
      EXPECT_EQ(expected_coupon_info.size(), coupon_infos.size());
      std::vector<
          testing::Matcher<std::unique_ptr<autofill::AutofillOfferData>>>
          expected_offer_data_matchers;
      for (const auto& offer_data : expected_coupon_info) {
        expected_offer_data_matchers.push_back(
            testing::Pointee(EqualsAutofillOfferData(*offer_data)));
      }
      EXPECT_THAT(coupon_infos, ElementsAreArray(expected_offer_data_matchers));
    }
  }
};

class FakeVariationsClient : public variations::VariationsClient {
 public:
  bool IsOffTheRecord() const override { return false; }

  variations::mojom::VariationsHeadersPtr GetVariationsHeaders()
      const override {
    base::flat_map<variations::mojom::GoogleWebVisibility, std::string>
        headers = {
            {variations::mojom::GoogleWebVisibility::FIRST_PARTY, "xyz456"}};
    return variations::mojom::VariationsHeaders::New(headers);
  }
};

class FetchDiscountWorkerTestBase : public testing::Test {
 public:
  void SetUp() override {
    test_shared_url_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);

    mock_fetcher_ = std::make_unique<MockCartDiscountFetcher>();

    TestingProfile::Builder profile_builder;
    profile_builder.AddTestingFactory(
        HistoryServiceFactory::GetInstance(),
        HistoryServiceFactory::GetDefaultFactory());
    profile_ = profile_builder.Build();
    fake_cart_service_delegate_ =
        std::make_unique<FakeCartDiscountServiceDelegate>(
            CartServiceFactory::GetForProfile(profile_.get()));

    fake_variations_client_ = std::make_unique<FakeVariationsClient>();
  }

  void TearDown() override {
    FakeCartDiscountFetcher::ClearFetchCount();
    is_signin_and_sync_ = false;

    auto& data = commerce_heuristics::CommerceHeuristicsData::GetInstance();
    data.PopulateDataFromComponent("{}", "{}", "", "");
  }

  // This method transfers mock_fetcher_ ownership. Set all expectations for
  // mock_fetcher_ before calling this method.
  void CreateCartDiscountFetcherFactory(
      CartDiscountFetcher::CartDiscountMap fetcher_fake_result,
      bool is_tester) {
    auto factory = std::make_unique<FakeCartDiscountFetcherFactory>();
    factory->SetFetcherFakeResult(std::move(fetcher_fake_result));
    factory->SetExpectedTester(is_tester);
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
        std::move(fake_cart_service_delegate_),
        is_signin_and_sync_ ? identity_test_env_.identity_manager() : nullptr,
        fake_variations_client_.get());
  }

  void SignInAndSync() {
    identity_test_env_.MakePrimaryAccountAvailable(kEmail,
                                                   signin::ConsentLevel::kSync);
    identity_test_env_.SetAutomaticIssueOfAccessTokens(true);
    is_signin_and_sync_ = true;
  }

  void CreateFakeFetchedResult() {}

 protected:
  // This needs to be destroyed after task_environment, so that any tasks on
  // other threads that might check if features are enabled complete first.
  base::test::ScopedFeatureList features_;
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  signin::IdentityTestEnvironment identity_test_env_;

  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory>
      test_shared_url_loader_factory_;
  std::unique_ptr<FetchDiscountWorker> fetch_discount_worker_;
  std::unique_ptr<CartDiscountFetcherFactory>
      fake_cart_discount_fetcher_factory_;

  std::unique_ptr<MockCartDiscountFetcher> mock_fetcher_;

  std::unique_ptr<FakeCartDiscountServiceDelegate> fake_cart_service_delegate_;

  std::unique_ptr<FakeVariationsClient> fake_variations_client_;

  std::unique_ptr<TestingProfile> profile_;
  bool is_signin_and_sync_ = false;
};

class FetchDiscountWorkerTest : public FetchDiscountWorkerTestBase {
 public:
  FetchDiscountWorkerTest() {
    std::vector<base::test::FeatureRefAndParams> enabled_features;
    base::FieldTrialParams cart_params, coupon_params;
    cart_params["NtpChromeCartModuleAbandonedCartDiscountParam"] = "true";
    cart_params["discount-fetch-delay"] = "6h";
    enabled_features.emplace_back(ntp_features::kNtpChromeCartModule,
                                  cart_params);
    coupon_params[commerce::kRetailCouponsWithCodeParam] = "true";
    enabled_features.emplace_back(commerce::kRetailCoupons, coupon_params);
    features_.InitWithFeaturesAndParameters(enabled_features,
                                            /*disabled_features*/ {});
  }

  void SetUp() override {
    FetchDiscountWorkerTestBase::SetUp();

    auto& data = commerce_heuristics::CommerceHeuristicsData::GetInstance();
    ASSERT_TRUE(data.PopulateDataFromComponent("{}", R"###(
        {
          "rule_discount_partner_merchant_regex": "(foo.com)",
          "coupon_discount_partner_merchant_regex": "(qux.com)"
        }
    )###",
                                               "", ""));
  }
};

TEST_F(FetchDiscountWorkerTest, TestStart_EndToEnd) {
  EXPECT_EQ(profile_->GetPrefs()->GetTime(prefs::kCartDiscountLastFetchedTime),
            base::Time());
  CartDiscountFetcher::CartDiscountMap fake_result;
  CreateCartDiscountFetcherFactory(std::move(fake_result), false);
  CreateWorker();

  fetch_discount_worker_->Start(base::Milliseconds(0));
  task_environment_.RunUntilIdle();
  EXPECT_NE(profile_->GetPrefs()->GetTime(prefs::kCartDiscountLastFetchedTime),
            base::Time());
}

TEST_F(FetchDiscountWorkerTest, TestStart_DiscountUpdatedWithRBDDiscount) {
  CartDiscountFetcher::CartDiscountMap fake_result;
  fake_result.emplace(
      kMockMerchantACartUrl,
      MerchantIdAndDiscounts(
          kMockMerchantAId, kMockMerchantADiscounts, kEmptyCouponDiscountList,
          kMockMerchantAHighestPercentOff, false /*has_coupons*/));
  CreateCartDiscountFetcherFactory(std::move(fake_result), false);

  CartDB::KeyAndValue mockMerchantACartContentKeyAndProto =
      std::make_pair(kMockMerchantA, kMockMerchantACartContentProto);
  std::vector<CartDB::KeyAndValue> loader_fake_data(
      1, mockMerchantACartContentKeyAndProto);
  fake_cart_service_delegate_->SetCartLoadFakeData(loader_fake_data);

  cart_db::ChromeCartContentProto cart_content_proto = BuildCartContentProto(
      kMockMerchantA, kMockMerchantACartUrl, kMockMerchantATimestamp);
  cart_db::ChromeCartContentProto updater_expected_data = AddRBDDiscountToProto(
      cart_content_proto, kMockMerchantAId, kMockMerchantADiscounts[0]);
  fake_cart_service_delegate_->SetCartDiscountUpdateExpectedData(
      updater_expected_data, true, kMockMerchantAHighestPercentOff);

  CreateWorker();

  fetch_discount_worker_->Start(base::Milliseconds(0));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(1, FakeCartDiscountFetcher::GetFetchCount());
}

TEST_F(FetchDiscountWorkerTest, TestStart_DiscountUpdatedWithCouponDiscount) {
  CartDiscountFetcher::CartDiscountMap fake_result;
  fake_result.emplace(
      kMockMerchantACartUrl,
      MerchantIdAndDiscounts(kMockMerchantAId, {}, kEmptyCouponDiscountList,
                             kMockMerchantAHighestPercentOff,
                             true /*has_coupons*/));
  CreateCartDiscountFetcherFactory(std::move(fake_result), false);

  CartDB::KeyAndValue mockMerchantACartContentKeyAndProto =
      std::make_pair(kMockMerchantA, kMockMerchantACartContentProto);
  std::vector<CartDB::KeyAndValue> loader_fake_data(
      1, mockMerchantACartContentKeyAndProto);
  fake_cart_service_delegate_->SetCartLoadFakeData(loader_fake_data);

  cart_db::ChromeCartContentProto cart_content_proto = BuildCartContentProto(
      kMockMerchantA, kMockMerchantACartUrl, kMockMerchantATimestamp);

  cart_db::ChromeCartContentProto updater_expected_data =
      AddCouponDiscountToProto(cart_content_proto, kMockMerchantAId,
                               true /*has_coupons*/);

  fake_cart_service_delegate_->SetCartDiscountUpdateExpectedData(
      updater_expected_data, false, kMockMerchantAHighestPercentOff, true);

  CreateWorker();

  fetch_discount_worker_->Start(base::Milliseconds(0));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(1, FakeCartDiscountFetcher::GetFetchCount());
}

TEST_F(FetchDiscountWorkerTest, TestStart_DiscountUpdatedClearDiscount) {
  // No discount available.
  CartDiscountFetcher::CartDiscountMap fake_result;
  CreateCartDiscountFetcherFactory(std::move(fake_result), false);

  // Loader fake data contatins discount.
  cart_db::ChromeCartContentProto cart_content_proto = BuildCartContentProto(
      kMockMerchantA, kMockMerchantACartUrl, kMockMerchantATimestamp);
  cart_db::ChromeCartContentProto cart_with_discount = AddRBDDiscountToProto(
      cart_content_proto, kMockMerchantAId, kMockMerchantADiscounts[0]);
  CartDB::KeyAndValue mockMerchantACartContentKeyAndProto =
      std::make_pair(kMockMerchantA, cart_with_discount);
  std::vector<CartDB::KeyAndValue> loader_fake_data(
      1, mockMerchantACartContentKeyAndProto);
  fake_cart_service_delegate_->SetCartLoadFakeData(loader_fake_data);

  // Updater is expected data without discount.
  cart_db::ChromeCartContentProto updater_expected_data = BuildCartContentProto(
      kMockMerchantA, kMockMerchantACartUrl, kMockMerchantATimestamp);
  fake_cart_service_delegate_->SetCartDiscountUpdateExpectedData(
      updater_expected_data, false);

  CreateWorker();

  fetch_discount_worker_->Start(base::Milliseconds(0));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(1, FakeCartDiscountFetcher::GetFetchCount());
}

TEST_F(FetchDiscountWorkerTest, TestStart_FetcherRefetched) {
  CartDiscountFetcher::CartDiscountMap fake_result;
  fake_result.emplace(
      kMockMerchantACartUrl,
      MerchantIdAndDiscounts(
          kMockMerchantAId, kMockMerchantADiscounts, kEmptyCouponDiscountList,
          kMockMerchantAHighestPercentOff, false /*has_coupons*/));
  CreateCartDiscountFetcherFactory(std::move(fake_result), false);

  CartDB::KeyAndValue mockMerchantACartContentKeyAndProto =
      std::make_pair(kMockMerchantA, kMockMerchantACartContentProto);
  std::vector<CartDB::KeyAndValue> loader_fake_data(
      1, mockMerchantACartContentKeyAndProto);
  fake_cart_service_delegate_->SetCartLoadFakeData(loader_fake_data);

  cart_db::ChromeCartContentProto cart_content_proto = BuildCartContentProto(
      kMockMerchantA, kMockMerchantACartUrl, kMockMerchantATimestamp);
  cart_db::ChromeCartContentProto updater_expected_data = AddRBDDiscountToProto(
      cart_content_proto, kMockMerchantAId, kMockMerchantADiscounts[0]);
  fake_cart_service_delegate_->SetCartDiscountUpdateExpectedData(
      updater_expected_data, true, kMockMerchantAHighestPercentOff);

  CreateWorker();

  fetch_discount_worker_->Start(base::Milliseconds(0));
  task_environment_.FastForwardBy(base::Milliseconds(1));
  EXPECT_EQ(1, FakeCartDiscountFetcher::GetFetchCount());

  task_environment_.FastForwardBy(base::Hours(7));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(2, FakeCartDiscountFetcher::GetFetchCount());
}

TEST_F(FetchDiscountWorkerTest, TestTesterFetch) {
  SignInAndSync();
  bool expected_a_tester = true;
  CartDiscountFetcher::CartDiscountMap fake_result;
  fake_result.emplace(
      kMockMerchantACartUrl,
      MerchantIdAndDiscounts(
          kMockMerchantAId, kMockMerchantADiscounts, kEmptyCouponDiscountList,
          kMockMerchantAHighestPercentOff, false /*has_coupons*/));
  CreateCartDiscountFetcherFactory(std::move(fake_result), expected_a_tester);

  CartDB::KeyAndValue mockMerchantACartContentKeyAndProto =
      std::make_pair(kMockMerchantA, kMockMerchantACartContentProto);
  std::vector<CartDB::KeyAndValue> loader_fake_data(
      1, mockMerchantACartContentKeyAndProto);
  fake_cart_service_delegate_->SetCartLoadFakeData(loader_fake_data);

  cart_db::ChromeCartContentProto cart_content_proto = BuildCartContentProto(
      kMockMerchantA, kMockMerchantACartUrl, kMockMerchantATimestamp);
  cart_db::ChromeCartContentProto updater_expected_data = AddRBDDiscountToProto(
      cart_content_proto, kMockMerchantAId, kMockMerchantADiscounts[0]);
  fake_cart_service_delegate_->SetCartDiscountUpdateExpectedData(
      updater_expected_data, true, kMockMerchantAHighestPercentOff);
  fake_cart_service_delegate_->SetExpectedTester(expected_a_tester);

  CreateWorker();

  fetch_discount_worker_->Start(base::Milliseconds(0));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(1, FakeCartDiscountFetcher::GetFetchCount());
}

TEST_F(FetchDiscountWorkerTest, TestFetchSkippedForNonPartnerMerchants) {
  // Mock that there is a non-partner-merchant cart.
  const char mock_merchant[] = "bar.com";
  const char mock_merchant_url[] = "https://www.bar.com/cart";
  const cart_db::ChromeCartContentProto mock_merchant_cart_proto =
      BuildCartContentProto(mock_merchant, mock_merchant_url,
                            kMockMerchantATimestamp);

  CartDiscountFetcher::CartDiscountMap fake_result;
  CreateCartDiscountFetcherFactory(std::move(fake_result), false);

  CartDB::KeyAndValue mockMerchantACartContentKeyAndProto =
      std::make_pair(mock_merchant, mock_merchant_cart_proto);
  std::vector<CartDB::KeyAndValue> loader_fake_data(
      1, mockMerchantACartContentKeyAndProto);
  fake_cart_service_delegate_->SetCartLoadFakeData(loader_fake_data);

  CreateWorker();

  fetch_discount_worker_->Start(base::Milliseconds(0));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(0, FakeCartDiscountFetcher::GetFetchCount());
}

TEST_F(FetchDiscountWorkerTest, TestFetchForCouponPartnerMerchants) {
  // Mock that there is a coupon partner merchant cart.
  const char mock_merchant[] = "qux.com";
  const char mock_merchant_url[] = "https://www.qux.com/cart";
  const cart_db::ChromeCartContentProto mock_merchant_cart_proto =
      BuildCartContentProto(mock_merchant, mock_merchant_url,
                            kMockMerchantATimestamp);

  CartDiscountFetcher::CartDiscountMap fake_result;
  CreateCartDiscountFetcherFactory(std::move(fake_result), false);

  CartDB::KeyAndValue mockMerchantACartContentKeyAndProto =
      std::make_pair(mock_merchant, mock_merchant_cart_proto);
  std::vector<CartDB::KeyAndValue> loader_fake_data(
      1, mockMerchantACartContentKeyAndProto);
  fake_cart_service_delegate_->SetCartLoadFakeData(loader_fake_data);
  fake_cart_service_delegate_->SetCartDiscountUpdateExpectedData(
      mock_merchant_cart_proto, false);

  CreateWorker();

  fetch_discount_worker_->Start(base::Milliseconds(0));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(1, FakeCartDiscountFetcher::GetFetchCount());
}

TEST_F(FetchDiscountWorkerTest, TestUpdateFreeListingCouponsWithCode) {
  coupon_db::FreeListingCouponInfoProto coupon_info_proto =
      BuildFreeListingCouponInfoProto("des", "code", 1, 1);

  std::vector<coupon_db::FreeListingCouponInfoProto> coupon_discount_list = {
      coupon_info_proto};
  CartDiscountFetcher::CartDiscountMap fake_result;
  fake_result.emplace(
      kMockMerchantACartUrl,
      MerchantIdAndDiscounts(
          kMockMerchantAId, kMockMerchantADiscounts, coupon_discount_list,
          kMockMerchantAHighestPercentOff, false /*has_coupons*/));
  CreateCartDiscountFetcherFactory(std::move(fake_result), false);

  CouponService::CouponsMap expected_map;
  expected_map[GURL(kMockMerchantACartUrl).DeprecatedGetOriginAsURL()]
      .emplace_back(BuildCouponsMapValueEntry(
          GURL(kMockMerchantACartUrl).DeprecatedGetOriginAsURL(),
          coupon_info_proto));
  fake_cart_service_delegate_->SetExpectedCouponMap(std::move(expected_map));

  CartDB::KeyAndValue mockMerchantACartContentKeyAndProto =
      std::make_pair(kMockMerchantA, kMockMerchantACartContentProto);
  std::vector<CartDB::KeyAndValue> loader_fake_data(
      1, mockMerchantACartContentKeyAndProto);
  fake_cart_service_delegate_->SetCartLoadFakeData(loader_fake_data);

  cart_db::ChromeCartContentProto cart_content_proto = BuildCartContentProto(
      kMockMerchantA, kMockMerchantACartUrl, kMockMerchantATimestamp);
  cart_db::ChromeCartContentProto updater_expected_data = AddRBDDiscountToProto(
      cart_content_proto, kMockMerchantAId, kMockMerchantADiscounts[0]);
  fake_cart_service_delegate_->SetCartDiscountUpdateExpectedData(
      updater_expected_data, true, kMockMerchantAHighestPercentOff);

  CreateWorker();

  fetch_discount_worker_->Start(base::Milliseconds(0));
  task_environment_.RunUntilIdle();
}

class FetchMerchantWideDiscountWorkerTest : public FetchDiscountWorkerTestBase {
 public:
  void SetUp() override {
    FetchDiscountWorkerTestBase::SetUp();

    auto& data = commerce_heuristics::CommerceHeuristicsData::GetInstance();
    ASSERT_TRUE(data.PopulateDataFromComponent("{}", kGlobalHeuristicsJSONData,
                                               "", ""));
  }
};

TEST_F(FetchMerchantWideDiscountWorkerTest, TestFetch) {
  const char mock_merchant[] = "bar.com";
  const char mock_merchant_url[] = "https://www.bar.com/cart";
  const cart_db::ChromeCartContentProto mock_merchant_cart_proto =
      BuildCartContentProto(mock_merchant, mock_merchant_url,
                            kMockMerchantATimestamp);

  CartDiscountFetcher::CartDiscountMap fake_result;
  CreateCartDiscountFetcherFactory(std::move(fake_result), false);

  CartDB::KeyAndValue mockMerchantACartContentKeyAndProto =
      std::make_pair(mock_merchant, mock_merchant_cart_proto);
  std::vector<CartDB::KeyAndValue> loader_fake_data(
      1, mockMerchantACartContentKeyAndProto);
  fake_cart_service_delegate_->SetCartLoadFakeData(loader_fake_data);

  CreateWorker();

  fetch_discount_worker_->Start(base::Milliseconds(0));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(1, FakeCartDiscountFetcher::GetFetchCount());
}

TEST_F(FetchMerchantWideDiscountWorkerTest,
       TestNoFetchForMerchantWithoutDiscounts) {
  const char mock_merchant[] = "nodiscount.com";
  const char mock_merchant_url[] = "https://www.nodiscount.com/cart";
  const cart_db::ChromeCartContentProto mock_merchant_cart_proto =
      BuildCartContentProto(mock_merchant, mock_merchant_url,
                            kMockMerchantATimestamp);

  CartDiscountFetcher::CartDiscountMap fake_result;
  CreateCartDiscountFetcherFactory(std::move(fake_result), false);

  CartDB::KeyAndValue mockMerchantACartContentKeyAndProto =
      std::make_pair(mock_merchant, mock_merchant_cart_proto);
  std::vector<CartDB::KeyAndValue> loader_fake_data(
      1, mockMerchantACartContentKeyAndProto);
  fake_cart_service_delegate_->SetCartLoadFakeData(loader_fake_data);

  CreateWorker();

  fetch_discount_worker_->Start(base::Milliseconds(0));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(0, FakeCartDiscountFetcher::GetFetchCount());
}
