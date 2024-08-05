// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cart/cart_service.h"

#include <optional>
#include <string_view>

#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/cart/cart_discount_metric_collector.h"
#include "chrome/browser/cart/cart_service_factory.h"
#include "chrome/browser/cart/fetch_discount_worker.h"
#include "chrome/browser/commerce/coupons/coupon_service.h"
#include "chrome/browser/commerce/shopping_service_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/persisted_state_db/session_proto_db_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/commerce_heuristics_data.h"
#include "components/commerce/core/commerce_heuristics_data_metrics_helper.h"
#include "components/commerce/core/mock_shopping_service.h"
#include "components/commerce/core/proto/cart_db_content.pb.h"
#include "components/commerce/core/shopping_service.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/search/ntp_features.h"
#include "components/session_proto_db/session_proto_db.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
cart_db::ChromeCartContentProto BuildProto(const char* domain,
                                           const char* merchant_url) {
  cart_db::ChromeCartContentProto proto;
  proto.set_key(domain);
  proto.set_merchant_cart_url(merchant_url);
  proto.set_timestamp(base::Time::Now().InSecondsFSinceUnixEpoch());
  return proto;
}

cart_db::ChromeCartContentProto BuildProtoWithProducts(
    const char* domain,
    const char* cart_url,
    const std::vector<const char*>& product_urls) {
  cart_db::ChromeCartContentProto proto;
  proto.set_key(domain);
  proto.set_merchant_cart_url(cart_url);
  proto.set_timestamp(base::Time::Now().InSecondsFSinceUnixEpoch());
  for (const auto* const v : product_urls) {
    proto.add_product_image_urls(v);
  }
  return proto;
}

cart_db::ChromeCartProductProto BuildProductProto(
    const std::string& product_id) {
  cart_db::ChromeCartProductProto proto;
  proto.set_product_id(product_id);
  return proto;
}

cart_db::ChromeCartContentProto AddDiscountToProto(
    cart_db::ChromeCartContentProto proto,
    const double timestamp,
    const std::string& rule_id,
    const int percent_off,
    const char* offer_id) {
  proto.mutable_discount_info()->set_last_fetched_timestamp(timestamp);
  cart_db::RuleDiscountInfoProto* added_discount =
      proto.mutable_discount_info()->add_rule_discount_info();
  added_discount->set_rule_id(rule_id);
  added_discount->set_percent_off(percent_off);
  added_discount->set_raw_merchant_offer_id(offer_id);
  return proto;
}

cart_db::ChromeCartContentProto AddCouponDiscountToProto(
    cart_db::ChromeCartContentProto proto,
    const double timestamp,
    const char* discount_text) {
  proto.mutable_discount_info()->set_last_fetched_timestamp(timestamp);
  proto.mutable_discount_info()->set_discount_text(discount_text);
  proto.mutable_discount_info()->set_has_coupons(true);
  return proto;
}

cart_db::ChromeCartContentProto AddCouponDiscountToProto(
    cart_db::ChromeCartContentProto proto,
    const double timestamp,
    const char* discount_text,
    const char* promo_id) {
  proto.mutable_discount_info()->set_last_fetched_timestamp(timestamp);
  proto.mutable_discount_info()->set_discount_text(discount_text);
  proto.mutable_discount_info()->set_has_coupons(true);

  cart_db::CouponInfoProto coupon_info;
  coupon_info.set_promo_id(promo_id);
  std::vector<cart_db::CouponInfoProto> coupon_info_protos;
  coupon_info_protos.emplace_back(coupon_info);
  *proto.mutable_discount_info()->mutable_coupon_info() = {
      coupon_info_protos.begin(), coupon_info_protos.end()};
  return proto;
}

MATCHER_P(EqualsProto, message, "") {
  std::string expected_serialized, actual_serialized;
  message.SerializeToString(&expected_serialized);
  arg.SerializeToString(&actual_serialized);
  return expected_serialized == actual_serialized;
}

constexpr char kFakeDataPrefix[] = "Fake:";
const char kMockMerchantA[] = "foo.com";
const char kMockMerchantURLA[] = "https://www.foo.com";
const char kMockMerchantURLWithDiscountUtmA[] =
    "https://www.foo.com/"
    "?utm_source=chrome&utm_medium=app&utm_campaign=chrome-cart-discount-on";
const char kMockMerchantURLWithNoDiscountUtmA[] =
    "https://www.foo.com/"
    "?utm_source=chrome&utm_medium=app&utm_campaign=chrome-cart-discount-off";
const char kMockMerchantB[] = "bar.com";
const char kMockMerchantURLB[] = "https://www.bar.com";
const char kMockMerchantC[] = "baz.com";
const char kMockMerchantURLC[] = "https://www.baz.com";
const char kMockMerchantURLWithCartUtmC[] =
    "https://www.baz.com/"
    "?utm_source=chrome&utm_medium=app&utm_campaign=chrome-cart";
const char kNoDiscountMerchant[] = "nodiscount.com";
const char kNoDiscountMerchantURL[] = "https://www.nodiscount.com";
const char kProductURL[] = "https://www.product.com";
const char kCommerceHintHeuristicsJSONData[] = R"###(
      {
          "foo.com": {
              "merchant_name": "Foo",
              "cart_url": "https://foo.com/cart"
          },
          "bar.com": {
              "merchant_name": "Bar"
          }
      }
  )###";
const char kGlobalHeuristicsJSONData[] = R"###(
      {
        "no_discount_merchant_regex": "nodiscount"
      }
)###";
const cart_db::ChromeCartContentProto kMockProtoA =
    BuildProto(kMockMerchantA, kMockMerchantURLA);
const cart_db::ChromeCartContentProto kMockProtoB =
    BuildProto(kMockMerchantB, kMockMerchantURLB);
const cart_db::ChromeCartContentProto kMockProtoC =
    BuildProto(kMockMerchantC, kMockMerchantURLC);
const cart_db::ChromeCartContentProto kMockProtoCWithProduct =
    BuildProtoWithProducts(kMockMerchantC, kMockMerchantURLC, {kProductURL});
using ShoppingCarts =
    std::vector<SessionProtoDB<cart_db::ChromeCartContentProto>::KeyAndValue>;
using ProductInfos = std::vector<cart_db::ChromeCartProductProto>;
const ShoppingCarts kExpectedA = {{kMockMerchantA, kMockProtoA}};
const ShoppingCarts kExpectedB = {{kMockMerchantB, kMockProtoB}};
const ShoppingCarts kExpectedAB = {
    {kMockMerchantB, kMockProtoB},
    {kMockMerchantA, kMockProtoA},
};
const ShoppingCarts kExpectedC = {{kMockMerchantC, kMockProtoC}};
const ShoppingCarts kExpectedCWithProduct = {
    {kMockMerchantC, kMockProtoCWithProduct}};
const ShoppingCarts kEmptyExpected = {};
const cart_db::ChromeCartProductProto kMockProductA =
    BuildProductProto("id_foo");
const cart_db::ChromeCartProductProto kMockProductB =
    BuildProductProto("id_bar");

// Value used for discount.
const char kMockMerchantADiscountRuleId[] = "1";
const char kMockMerchantADiscountPromoId[] = "2";
const int kMockMerchantADiscountsPercentOff = 10;
const char kMockMerchantADiscountsRawMerchantOfferId[] = "merchantAOfferId";
const bool kNotATester = false;
const bool kATester = true;

std::vector<cart_db::RuleDiscountInfoProto> BuildMerchantADiscountInfoProtos() {
  cart_db::RuleDiscountInfoProto proto;
  proto.set_rule_id(kMockMerchantADiscountRuleId);
  proto.set_percent_off(kMockMerchantADiscountsPercentOff);
  proto.set_raw_merchant_offer_id(kMockMerchantADiscountsRawMerchantOfferId);

  return std::vector<cart_db::RuleDiscountInfoProto>(1, proto);
}
const std::vector<cart_db::RuleDiscountInfoProto> kMockMerchantADiscounts =
    BuildMerchantADiscountInfoProtos();

}  // namespace

class CartServiceTest : public testing::Test {
 public:
  CartServiceTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        mock_merchant_url_A_(kMockMerchantURLA),
        mock_merchant_url_B_(kMockMerchantURLB),
        mock_merchant_url_C_(kMockMerchantURLC) {}

  void SetUp() override {
    testing::Test::SetUp();

    TestingProfile::Builder profile_builder;
    profile_builder.AddTestingFactory(
        HistoryServiceFactory::GetInstance(),
        HistoryServiceFactory::GetDefaultFactory());
    profile_builder.AddTestingFactory(
        commerce::ShoppingServiceFactory::GetInstance(),
        base::BindRepeating([](content::BrowserContext* context) {
          return commerce::MockShoppingService::Build();
        }));
    profile_ = profile_builder.Build();

    service_ = CartServiceFactory::GetForProfile(profile_.get());
  }

  void OperationEvaluation(base::OnceClosure closure,
                           bool expected_success,
                           bool actual_success) {
    GetEvaluationBoolResult(std::move(closure), expected_success,
                            actual_success);
  }

  void GetEvaluationBoolResult(base::OnceClosure closure,
                               bool expected,
                               bool actual) {
    EXPECT_EQ(expected, actual);
    std::move(closure).Run();
  }

  void GetEvaluationURL(base::OnceClosure closure,
                        ShoppingCarts expected,
                        bool result,
                        ShoppingCarts found) {
    EXPECT_EQ(found.size(), expected.size());
    for (size_t i = 0; i < expected.size(); i++) {
      EXPECT_EQ(found[i].first, expected[i].first);
      EXPECT_EQ(found[i].second.merchant(), expected[i].second.merchant());
      EXPECT_EQ(found[i].second.merchant_cart_url(),
                expected[i].second.merchant_cart_url());
      for (int j = 0; j < expected[i].second.product_image_urls().size(); j++) {
        EXPECT_EQ(expected[i].second.product_image_urls()[j],
                  found[i].second.product_image_urls()[j]);
      }
    }
    std::move(closure).Run();
  }

  std::string GetCartURL(const std::string& domain) {
    base::RunLoop run_loop;
    std::string cart_url;

    service_->LoadCart(
        domain, base::BindOnce(
                    [](base::OnceClosure closure, std::string* out, bool result,
                       ShoppingCarts found) {
                      EXPECT_TRUE(result);
                      EXPECT_EQ(found.size(), 1U);
                      *out = found[0].second.merchant_cart_url();
                      std::move(closure).Run();
                    },
                    run_loop.QuitClosure(), &cart_url));
    run_loop.Run();
    return cart_url;
  }

  void GetEvaluationFakeDataDB(base::OnceClosure closure,
                               bool result,
                               ShoppingCarts found) {
    EXPECT_EQ(found.size(), 6U);
    for (CartDB::KeyAndValue proto_pair : found) {
      EXPECT_EQ(proto_pair.second.key().rfind(kFakeDataPrefix, 0), 0U);
    }
    std::move(closure).Run();
  }

  void GetEvaluationCartHiddenStatus(base::OnceClosure closure,
                                     bool isHidden,
                                     bool result,
                                     ShoppingCarts found) {
    EXPECT_EQ(1U, found.size());
    EXPECT_EQ(isHidden, found[0].second.is_hidden());
    std::move(closure).Run();
  }

  void GetEvaluationCartRemovedStatus(base::OnceClosure closure,
                                      bool isRemoved,
                                      bool result,
                                      ShoppingCarts found) {
    EXPECT_EQ(1U, found.size());
    EXPECT_EQ(isRemoved, found[0].second.is_removed());
    std::move(closure).Run();
  }

  void GetEvaluationCartTimeStamp(base::OnceClosure closure,
                                  double expect_timestamp,
                                  bool result,
                                  ShoppingCarts found) {
    EXPECT_EQ(1U, found.size());
    EXPECT_EQ(expect_timestamp, found[0].second.timestamp());
    std::move(closure).Run();
  }

  void GetEvaluationProductInfo(base::OnceClosure closure,
                                ProductInfos expected_products,
                                bool result,
                                ShoppingCarts found_carts) {
    EXPECT_EQ(1U, found_carts.size());
    auto found_products = found_carts[0].second.product_infos();
    EXPECT_EQ((size_t)found_products.size(), expected_products.size());
    for (size_t i = 0; i < expected_products.size(); i++) {
      EXPECT_EQ(found_products.at(i).product_id(),
                expected_products[i].product_id());
    }
    std::move(closure).Run();
  }

  void GetEvaluationEmptyDiscount(base::OnceClosure closure,
                                  bool result,
                                  ShoppingCarts found) {
    EXPECT_EQ(found.size(), 1U);
    cart_db::ChromeCartContentProto cart = found[0].second;
    EXPECT_TRUE(!cart.has_discount_info() ||
                (cart.discount_info().rule_discount_info().empty() &&
                 cart.discount_info().coupon_info().empty() &&
                 !cart.discount_info().has_coupons()));
    std::move(closure).Run();
  }

  void GetEvaluationDiscount(base::OnceClosure closure,
                             ShoppingCarts expected,
                             bool result,
                             ShoppingCarts found) {
    EXPECT_EQ(found.size(), expected.size());
    EXPECT_EQ(found.size(), 1U);

    CartDB::KeyAndValue found_pair = found[0];
    CartDB::KeyAndValue expected_pair = expected[0];

    EXPECT_EQ(expected_pair.second.has_discount_info(),
              found_pair.second.has_discount_info());

    EXPECT_THAT(found_pair.second.discount_info(),
                EqualsProto(expected_pair.second.discount_info()));

    std::move(closure).Run();
  }

  void GetEvaluationDiscountURL(base::OnceClosure closure,
                                const GURL& expected,
                                const GURL& found) {
    EXPECT_EQ(expected, found);
    std::move(closure).Run();
  }

  std::string getDomainName(std::string_view domain) {
    std::string* res = service_->domain_name_mapping_.FindString(domain);
    if (!res)
      return "";
    return *res;
  }

  std::string getDomainCartURL(std::string_view domain) {
    std::string* res = service_->domain_cart_url_mapping_.FindString(domain);
    if (!res)
      return "";
    return *res;
  }

  void CacheUsedDiscounts(const cart_db::ChromeCartContentProto& proto) {
    service_->CacheUsedDiscounts(proto);
  }

  void CleanUpDiscounts(const cart_db::ChromeCartContentProto& proto) {
    service_->CleanUpDiscounts(proto);
  }

  base::flat_map<std::string, cart_db::ChromeCartContentProto>
  GetPendingDeletionMap() {
    return service_->pending_deletion_map_;
  }

  void TearDown() override {
    // Clean up the used discounts dictionary prefs.
    profile_->GetPrefs()->ClearPref(prefs::kCartUsedDiscounts);
    auto& data = commerce_heuristics::CommerceHeuristicsData::GetInstance();
    data.PopulateDataFromComponent("{}", "{}", "", "");
  }

 protected:
  // This needs to be destroyed after task_environment, so that any tasks on
  // other threads that might check if features are enabled complete first.
  base::test::ScopedFeatureList features_;

  // Required to run tests from UI thread.
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  raw_ptr<CartService> service_;
  base::HistogramTester histogram_tester_;
  const GURL mock_merchant_url_A_;
  const GURL mock_merchant_url_B_;
  const GURL mock_merchant_url_C_;
};

// Verifies the hide status is flipped by hiding and restoring.
TEST_F(CartServiceTest, TestHideStatusChange) {
  ASSERT_FALSE(service_->IsHidden());

  service_->Hide();
  ASSERT_TRUE(service_->IsHidden());

  service_->RestoreHidden();
  ASSERT_FALSE(service_->IsHidden());
}

// Tests adding one cart to the service.
TEST_F(CartServiceTest, TestAddCart) {
  CartDB* cart_db_ = service_->GetDB();
  base::RunLoop run_loop[2];
  cart_db_->LoadAllCarts(
      base::BindOnce(&CartServiceTest::GetEvaluationURL, base::Unretained(this),
                     run_loop[0].QuitClosure(), kEmptyExpected));
  run_loop[0].Run();

  service_->AddCart(mock_merchant_url_A_, std::nullopt, kMockProtoA);
  task_environment_.RunUntilIdle();

  cart_db_->LoadAllCarts(base::BindOnce(&CartServiceTest::GetEvaluationURL,
                                        base::Unretained(this),
                                        run_loop[1].QuitClosure(), kExpectedA));
  run_loop[1].Run();
}

// Test updating discount for one cart.
TEST_F(CartServiceTest, TestUpdateDiscounts) {
  CartDB* cart_db = service_->GetDB();
  cart_db::ChromeCartContentProto proto =
      BuildProto(kMockMerchantA, kMockMerchantURLA);

  base::RunLoop run_loop[4];
  cart_db->AddCart(
      kMockMerchantA, proto,
      base::BindOnce(&CartServiceTest::OperationEvaluation,
                     base::Unretained(this), run_loop[0].QuitClosure(), true));
  run_loop[0].Run();

  cart_db->LoadCart(
      kMockMerchantA,
      base::BindOnce(&CartServiceTest::GetEvaluationEmptyDiscount,
                     base::Unretained(this), run_loop[1].QuitClosure()));
  run_loop[1].Run();

  const double timestamp = 1;

  cart_db::ChromeCartContentProto cart_with_discount_proto =
      AddDiscountToProto(proto, timestamp, kMockMerchantADiscountRuleId,
                         kMockMerchantADiscountsPercentOff,
                         kMockMerchantADiscountsRawMerchantOfferId);

  service_->UpdateDiscounts(GURL(kMockMerchantURLA), cart_with_discount_proto,
                            kNotATester);

  const ShoppingCarts expected = {{kMockMerchantA, cart_with_discount_proto}};

  cart_db->LoadCart(kMockMerchantA,
                    base::BindOnce(&CartServiceTest::GetEvaluationDiscount,
                                   base::Unretained(this),
                                   run_loop[2].QuitClosure(), expected));
  run_loop[2].Run();

  CacheUsedDiscounts(cart_with_discount_proto);
  service_->UpdateDiscounts(GURL(kMockMerchantURLA), cart_with_discount_proto,
                            kNotATester);
  cart_db->LoadCart(
      kMockMerchantA,
      base::BindOnce(&CartServiceTest::GetEvaluationEmptyDiscount,
                     base::Unretained(this), run_loop[3].QuitClosure()));
  run_loop[3].Run();
}

// Test updating code-based RBD for one cart.
TEST_F(CartServiceTest, TestUpdateDiscounts_CodeBasedRBD) {
  base::test::ScopedFeatureList feature_list;
  std::vector<base::test::FeatureRefAndParams> enabled_features;
  base::FieldTrialParams code_based_rbd_param;
  code_based_rbd_param[commerce::kCodeBasedRuleDiscountParam] = "true";
  enabled_features.emplace_back(commerce::kCodeBasedRBD, code_based_rbd_param);
  feature_list.InitWithFeaturesAndParameters(enabled_features,
                                             /*disabled_features*/ {});

  CartDB* cart_db = service_->GetDB();
  cart_db::ChromeCartContentProto proto =
      BuildProto(kMockMerchantA, kMockMerchantURLA);

  base::RunLoop run_loop[3];
  cart_db->AddCart(
      kMockMerchantA, proto,
      base::BindOnce(&CartServiceTest::OperationEvaluation,
                     base::Unretained(this), run_loop[0].QuitClosure(), true));
  run_loop[0].Run();

  cart_db->LoadCart(
      kMockMerchantA,
      base::BindOnce(&CartServiceTest::GetEvaluationEmptyDiscount,
                     base::Unretained(this), run_loop[1].QuitClosure()));
  run_loop[1].Run();

  const double timestamp = 1;

  cart_db::ChromeCartContentProto cart_with_coupon_proto =
      AddCouponDiscountToProto(proto, timestamp,
                               /*discount_text=*/"10% off",
                               kMockMerchantADiscountPromoId);

  service_->UpdateDiscounts(GURL(kMockMerchantURLA), cart_with_coupon_proto,
                            kNotATester);

  const ShoppingCarts expected = {{kMockMerchantA, cart_with_coupon_proto}};

  cart_db->LoadCart(kMockMerchantA,
                    base::BindOnce(&CartServiceTest::GetEvaluationDiscount,
                                   base::Unretained(this),
                                   run_loop[2].QuitClosure(), expected));
  run_loop[2].Run();
}

// Test adding a cart with the same key and no product image won't overwrite
// existing proto.
TEST_F(CartServiceTest, TestAddCartWithNoProductImages) {
  CartDB* cart_db_ = service_->GetDB();
  base::RunLoop run_loop[3];
  cart_db::ChromeCartContentProto merchant_A_proto =
      BuildProto(kMockMerchantA, kMockMerchantURLA);
  merchant_A_proto.set_timestamp(0);
  merchant_A_proto.add_product_image_urls("https://image1.com");
  merchant_A_proto.set_is_hidden(true);
  service_->AddCart(mock_merchant_url_A_, std::nullopt, merchant_A_proto);
  task_environment_.RunUntilIdle();

  // Add a new proto with the same key and no product images.
  cart_db::ChromeCartContentProto new_proto =
      BuildProto(kMockMerchantA, kMockMerchantURLA);
  new_proto.set_timestamp(1);
  service_->AddCart(mock_merchant_url_A_, std::nullopt, new_proto);
  task_environment_.RunUntilIdle();

  cart_db_->LoadCart(
      kMockMerchantA,
      base::BindOnce(&CartServiceTest::GetEvaluationCartHiddenStatus,
                     base::Unretained(this), run_loop[0].QuitClosure(), false));
  run_loop[0].Run();
  cart_db_->LoadCart(
      kMockMerchantA,
      base::BindOnce(&CartServiceTest::GetEvaluationCartTimeStamp,
                     base::Unretained(this), run_loop[1].QuitClosure(), 1));
  run_loop[1].Run();
  const ShoppingCarts result = {{kMockMerchantA, merchant_A_proto}};
  cart_db_->LoadCart(
      kMockMerchantA,
      base::BindOnce(&CartServiceTest::GetEvaluationURL, base::Unretained(this),
                     run_loop[2].QuitClosure(), result));
  run_loop[2].Run();
}

// Test adding a cart with the same key and some product images would
// overwrite the product_image_url in the existing proto.
TEST_F(CartServiceTest, TestAddCartWithProductImages) {
  std::string merchant_A_discount_text = "10% off";
  double new_timestamp = 1.0;
  std::string new_product_image_url = "https://image2.com";

  CartDB* cart_db_ = service_->GetDB();
  base::RunLoop run_loop[3];
  cart_db::ChromeCartContentProto merchant_A_proto =
      BuildProto(kMockMerchantA, kMockMerchantURLA);
  merchant_A_proto.set_timestamp(0);
  merchant_A_proto.add_product_image_urls("https://image1.com");
  merchant_A_proto.mutable_discount_info()->set_discount_text(
      merchant_A_discount_text);
  merchant_A_proto.set_is_hidden(true);
  service_->AddCart(mock_merchant_url_A_, std::nullopt, merchant_A_proto);
  task_environment_.RunUntilIdle();

  // Add a new proto with the same key and some product images.
  cart_db::ChromeCartContentProto new_proto =
      BuildProto(kMockMerchantA, kMockMerchantURLA);
  new_proto.set_timestamp(new_timestamp);
  new_proto.add_product_image_urls(new_product_image_url);
  service_->AddCart(mock_merchant_url_A_, std::nullopt, new_proto);
  task_environment_.RunUntilIdle();

  cart_db_->LoadCart(
      kMockMerchantA,
      base::BindOnce(&CartServiceTest::GetEvaluationCartHiddenStatus,
                     base::Unretained(this), run_loop[0].QuitClosure(), false));
  run_loop[0].Run();
  cart_db_->LoadCart(
      kMockMerchantA,
      base::BindOnce(&CartServiceTest::GetEvaluationCartTimeStamp,
                     base::Unretained(this), run_loop[1].QuitClosure(),
                     new_timestamp));
  run_loop[1].Run();
  merchant_A_proto.set_timestamp(new_timestamp);
  merchant_A_proto.set_product_image_urls(0, new_product_image_url);
  merchant_A_proto.set_is_hidden(false);
  const ShoppingCarts result = {{kMockMerchantA, merchant_A_proto}};
  cart_db_->LoadCart(
      kMockMerchantA,
      base::BindOnce(&CartServiceTest::GetEvaluationURL, base::Unretained(this),
                     run_loop[2].QuitClosure(), result));
  run_loop[2].Run();
}

// Test adding a cart that has been removed would not take effect.
TEST_F(CartServiceTest, TestAddRemovedCart) {
  CartDB* cart_db_ = service_->GetDB();
  base::RunLoop run_loop[3];
  cart_db::ChromeCartContentProto merchant_A_proto =
      BuildProto(kMockMerchantA, kMockMerchantURLA);
  merchant_A_proto.set_timestamp(0);
  merchant_A_proto.add_product_image_urls("https://image1.com");
  merchant_A_proto.set_is_removed(true);
  service_->AddCart(mock_merchant_url_A_, std::nullopt, merchant_A_proto);
  task_environment_.RunUntilIdle();

  // Add a new proto with the same key and some product images.
  cart_db::ChromeCartContentProto new_proto =
      BuildProto(kMockMerchantA, kMockMerchantURLA);
  new_proto.set_timestamp(2);
  new_proto.add_product_image_urls("https://image2.com");
  service_->AddCart(mock_merchant_url_A_, std::nullopt, new_proto);
  task_environment_.RunUntilIdle();

  cart_db_->LoadCart(
      kMockMerchantA,
      base::BindOnce(&CartServiceTest::GetEvaluationCartRemovedStatus,
                     base::Unretained(this), run_loop[0].QuitClosure(), true));
  run_loop[0].Run();
  cart_db_->LoadCart(
      kMockMerchantA,
      base::BindOnce(&CartServiceTest::GetEvaluationCartTimeStamp,
                     base::Unretained(this), run_loop[1].QuitClosure(), 0));
  run_loop[1].Run();
  const ShoppingCarts result = {{kMockMerchantA, merchant_A_proto}};
  cart_db_->LoadCart(
      kMockMerchantA,
      base::BindOnce(&CartServiceTest::GetEvaluationURL, base::Unretained(this),
                     run_loop[2].QuitClosure(), result));
  run_loop[2].Run();
}

TEST_F(CartServiceTest, TestAddCartWithProductInfo) {
  base::RunLoop run_loop[6];
  CartDB* cart_db_ = service_->GetDB();
  cart_db::ChromeCartContentProto merchant_proto =
      BuildProto(kMockMerchantA, kMockMerchantURLA);
  merchant_proto.set_timestamp(0);
  merchant_proto.add_product_image_urls("https://image1.com");
  service_->AddCart(mock_merchant_url_A_, std::nullopt, merchant_proto);
  task_environment_.RunUntilIdle();

  // Adding a new proto with new product infos should reflect in storage.
  cart_db::ChromeCartContentProto new_proto =
      BuildProto(kMockMerchantA, kMockMerchantURLA);
  auto* added_product = new_proto.add_product_infos();
  *added_product = kMockProductA;
  new_proto.set_timestamp(1);
  service_->AddCart(mock_merchant_url_A_, std::nullopt, new_proto);
  task_environment_.RunUntilIdle();

  cart_db_->LoadCart(
      kMockMerchantA,
      base::BindOnce(&CartServiceTest::GetEvaluationCartTimeStamp,
                     base::Unretained(this), run_loop[1].QuitClosure(), 1));
  run_loop[1].Run();
  const ShoppingCarts& expected_carts = {{kMockMerchantA, merchant_proto}};
  cart_db_->LoadCart(
      kMockMerchantA,
      base::BindOnce(&CartServiceTest::GetEvaluationURL, base::Unretained(this),
                     run_loop[2].QuitClosure(), expected_carts));
  run_loop[2].Run();
  const ProductInfos& expected_products = {kMockProductA};
  cart_db_->LoadCart(
      kMockMerchantA,
      base::BindOnce(&CartServiceTest::GetEvaluationProductInfo,
                     base::Unretained(this), run_loop[3].QuitClosure(),
                     expected_products));
  run_loop[3].Run();

  // Adding a new proto with same product infos shouldn't change the current
  // storage about product infos.
  new_proto.set_timestamp(2);
  service_->AddCart(mock_merchant_url_A_, std::nullopt, new_proto);
  task_environment_.RunUntilIdle();

  cart_db_->LoadCart(
      kMockMerchantA,
      base::BindOnce(&CartServiceTest::GetEvaluationCartTimeStamp,
                     base::Unretained(this), run_loop[4].QuitClosure(), 2));
  run_loop[4].Run();
  cart_db_->LoadCart(
      kMockMerchantA,
      base::BindOnce(&CartServiceTest::GetEvaluationProductInfo,
                     base::Unretained(this), run_loop[5].QuitClosure(),
                     expected_products));
  run_loop[5].Run();
}

// Tests deleting one cart from the service.
TEST_F(CartServiceTest, TestDeleteCart) {
  CartDB* cart_db_ = service_->GetDB();
  base::RunLoop run_loop[4];
  cart_db::ChromeCartContentProto merchant_proto =
      BuildProto(kMockMerchantA, kMockMerchantURLA);
  merchant_proto.set_is_removed(true);
  cart_db_->AddCart(
      kMockMerchantA, merchant_proto,
      base::BindOnce(&CartServiceTest::OperationEvaluation,
                     base::Unretained(this), run_loop[0].QuitClosure(), true));
  run_loop[0].Run();

  cart_db_->LoadAllCarts(base::BindOnce(&CartServiceTest::GetEvaluationURL,
                                        base::Unretained(this),
                                        run_loop[1].QuitClosure(), kExpectedA));
  run_loop[1].Run();

  service_->DeleteCart(GURL(kMockMerchantURLA), false);

  cart_db_->LoadAllCarts(base::BindOnce(&CartServiceTest::GetEvaluationURL,
                                        base::Unretained(this),
                                        run_loop[2].QuitClosure(), kExpectedA));
  run_loop[2].Run();

  service_->DeleteCart(GURL(kMockMerchantURLA), true);

  cart_db_->LoadAllCarts(
      base::BindOnce(&CartServiceTest::GetEvaluationURL, base::Unretained(this),
                     run_loop[3].QuitClosure(), kEmptyExpected));
  run_loop[3].Run();
}

TEST_F(CartServiceTest, TestDeleteCart_NoPendingWhenRemoval) {
  CartDB* cart_db_ = service_->GetDB();
  base::RunLoop run_loop[2];
  cart_db::ChromeCartContentProto merchant_proto =
      BuildProto(kMockMerchantA, kMockMerchantURLA);
  merchant_proto.set_is_removed(true);
  cart_db_->AddCart(
      kMockMerchantA, merchant_proto,
      base::BindOnce(&CartServiceTest::OperationEvaluation,
                     base::Unretained(this), run_loop[0].QuitClosure(), true));
  run_loop[0].Run();

  // The cart deletion will not be tracked as pending when ignoring removal
  // status.
  service_->DeleteCart(GURL(kMockMerchantURLA), true);
  task_environment_.RunUntilIdle();

  cart_db_->LoadAllCarts(
      base::BindOnce(&CartServiceTest::GetEvaluationURL, base::Unretained(this),
                     run_loop[1].QuitClosure(), kEmptyExpected));
  run_loop[1].Run();
  EXPECT_FALSE(GetPendingDeletionMap().contains(kMockMerchantA));
}

TEST_F(CartServiceTest, TestDeleteCart_PendingDeletion) {
  CartDB* cart_db_ = service_->GetDB();
  base::RunLoop run_loop[3];
  const std::string discount_text = "15% off";
  // Build two protos who have the same products but the only difference is
  // discount info.
  cart_db::ChromeCartContentProto proto_with_discount =
      BuildProtoWithProducts(kMockMerchantA, kMockMerchantURLA, {kProductURL});
  proto_with_discount.mutable_discount_info()->set_discount_text(discount_text);
  cart_db::ChromeCartContentProto proto_without_discount =
      BuildProtoWithProducts(kMockMerchantA, kMockMerchantURLA, {kProductURL});

  cart_db_->AddCart(
      kMockMerchantA, proto_with_discount,
      base::BindOnce(&CartServiceTest::OperationEvaluation,
                     base::Unretained(this), run_loop[0].QuitClosure(), true));
  run_loop[0].Run();

  service_->DeleteCart(GURL(kMockMerchantURLA), false);
  task_environment_.RunUntilIdle();

  // The cart is deleted right away, but the deletion is cached in the pending
  // deletion map until the deletion is committed.
  cart_db_->LoadAllCarts(
      base::BindOnce(&CartServiceTest::GetEvaluationURL, base::Unretained(this),
                     run_loop[1].QuitClosure(), kEmptyExpected));
  run_loop[1].Run();
  task_environment_.FastForwardBy(
      commerce::kCodeBasedRuleDiscountCouponDeletionTime.Get() -
      base::Seconds(2));
  EXPECT_TRUE(GetPendingDeletionMap().contains(kMockMerchantA));

  // When deletion is pending, try to reuse the deleted cart proto.
  service_->AddCart(mock_merchant_url_A_, std::nullopt, proto_without_discount);
  task_environment_.RunUntilIdle();
  const ShoppingCarts expected = {{kMockMerchantA, proto_with_discount}};
  cart_db_->LoadAllCarts(base::BindOnce(&CartServiceTest::GetEvaluationURL,
                                        base::Unretained(this),
                                        run_loop[2].QuitClosure(), expected));
  run_loop[2].Run();
  EXPECT_FALSE(GetPendingDeletionMap().contains(kMockMerchantA));
}

TEST_F(CartServiceTest, TestDeleteCart_CommitPendingDeletion) {
  CartDB* cart_db_ = service_->GetDB();
  base::RunLoop run_loop[3];
  const std::string discount_text = "15% off";
  // Build two protos who have the same products but the only difference is
  // discount info.
  cart_db::ChromeCartContentProto proto_with_discount =
      BuildProtoWithProducts(kMockMerchantA, kMockMerchantURLA, {kProductURL});
  proto_with_discount.mutable_discount_info()->set_discount_text(discount_text);
  cart_db::ChromeCartContentProto proto_without_discount =
      BuildProtoWithProducts(kMockMerchantA, kMockMerchantURLA, {kProductURL});

  cart_db_->AddCart(
      kMockMerchantA, proto_with_discount,
      base::BindOnce(&CartServiceTest::OperationEvaluation,
                     base::Unretained(this), run_loop[0].QuitClosure(), true));
  run_loop[0].Run();

  service_->DeleteCart(GURL(kMockMerchantURLA), false);
  task_environment_.RunUntilIdle();

  // The cart is deleted right away, and the deletion is committed after
  // predefined period of time.
  cart_db_->LoadAllCarts(
      base::BindOnce(&CartServiceTest::GetEvaluationURL, base::Unretained(this),
                     run_loop[1].QuitClosure(), kEmptyExpected));
  run_loop[1].Run();
  task_environment_.FastForwardBy(
      commerce::kCodeBasedRuleDiscountCouponDeletionTime.Get());
  EXPECT_FALSE(GetPendingDeletionMap().contains(kMockMerchantA));

  // Deleted cart proto cannot be reused after deletion is committed.
  service_->AddCart(mock_merchant_url_A_, std::nullopt, proto_without_discount);
  task_environment_.RunUntilIdle();
  const ShoppingCarts expected = {{kMockMerchantA, proto_without_discount}};
  cart_db_->LoadAllCarts(base::BindOnce(&CartServiceTest::GetEvaluationURL,
                                        base::Unretained(this),
                                        run_loop[2].QuitClosure(), expected));
  run_loop[2].Run();
  EXPECT_FALSE(GetPendingDeletionMap().contains(kMockMerchantA));
}

TEST_F(CartServiceTest,
       TestDeleteCart_NotReusePendingDeletionForDifferentCart) {
  CartDB* cart_db_ = service_->GetDB();
  base::RunLoop run_loop[3];
  const std::string discount_text = "15% off";
  // Build two protos who have different products.
  cart_db::ChromeCartContentProto proto_with_productA = BuildProtoWithProducts(
      kMockMerchantA, kMockMerchantURLA, {"https://productA.com"});
  cart_db::ChromeCartContentProto proto_with_productB = BuildProtoWithProducts(
      kMockMerchantA, kMockMerchantURLA, {"https://productB.com"});

  cart_db_->AddCart(
      kMockMerchantA, proto_with_productA,
      base::BindOnce(&CartServiceTest::OperationEvaluation,
                     base::Unretained(this), run_loop[0].QuitClosure(), true));
  run_loop[0].Run();

  service_->DeleteCart(GURL(kMockMerchantURLA), false);
  task_environment_.RunUntilIdle();

  cart_db_->LoadAllCarts(
      base::BindOnce(&CartServiceTest::GetEvaluationURL, base::Unretained(this),
                     run_loop[1].QuitClosure(), kEmptyExpected));
  run_loop[1].Run();
  task_environment_.FastForwardBy(
      commerce::kCodeBasedRuleDiscountCouponDeletionTime.Get() -
      base::Seconds(2));
  EXPECT_TRUE(GetPendingDeletionMap().contains(kMockMerchantA));

  // Deleted cart proto cannot be reused if the new proto is different from the
  // deleted proto.
  service_->AddCart(mock_merchant_url_A_, std::nullopt, proto_with_productB);
  task_environment_.RunUntilIdle();
  const ShoppingCarts expected = {{kMockMerchantA, proto_with_productB}};
  cart_db_->LoadAllCarts(base::BindOnce(&CartServiceTest::GetEvaluationURL,
                                        base::Unretained(this),
                                        run_loop[2].QuitClosure(), expected));
  run_loop[2].Run();
  EXPECT_TRUE(GetPendingDeletionMap().contains(kMockMerchantA));

  // Deletion is committed after delay.
  task_environment_.FastForwardBy(base::Seconds(2));
  EXPECT_FALSE(GetPendingDeletionMap().contains(kMockMerchantA));
}

// Tests loading one cart from the service.
TEST_F(CartServiceTest, TestLoadCart) {
  CartDB* cart_db_ = service_->GetDB();
  base::RunLoop run_loop[3];
  cart_db_->AddCart(
      kMockMerchantA, kMockProtoA,
      base::BindOnce(&CartServiceTest::OperationEvaluation,
                     base::Unretained(this), run_loop[0].QuitClosure(), true));
  run_loop[0].Run();

  service_->LoadCart(
      kMockMerchantB,
      base::BindOnce(&CartServiceTest::GetEvaluationURL, base::Unretained(this),
                     run_loop[1].QuitClosure(), kEmptyExpected));
  run_loop[1].Run();

  service_->LoadCart(
      kMockMerchantA,
      base::BindOnce(&CartServiceTest::GetEvaluationURL, base::Unretained(this),
                     run_loop[2].QuitClosure(), kExpectedA));
  run_loop[2].Run();
}

// Tests loading all active carts from the service.
TEST_F(CartServiceTest, TestLoadAllActiveCarts) {
  CartDB* cart_db_ = service_->GetDB();
  base::RunLoop run_loop[8];
  cart_db_->AddCart(
      kMockMerchantA, kMockProtoA,
      base::BindOnce(&CartServiceTest::OperationEvaluation,
                     base::Unretained(this), run_loop[0].QuitClosure(), true));
  run_loop[0].Run();

  service_->LoadAllActiveCarts(
      base::BindOnce(&CartServiceTest::GetEvaluationURL, base::Unretained(this),
                     run_loop[1].QuitClosure(), kExpectedA));
  run_loop[1].Run();

  cart_db_->AddCart(
      kMockMerchantB, kMockProtoB,
      base::BindOnce(&CartServiceTest::OperationEvaluation,
                     base::Unretained(this), run_loop[2].QuitClosure(), true));
  run_loop[2].Run();

  service_->LoadAllActiveCarts(
      base::BindOnce(&CartServiceTest::GetEvaluationURL, base::Unretained(this),
                     run_loop[3].QuitClosure(), kExpectedAB));
  run_loop[3].Run();

  service_->HideCart(
      GURL(kMockMerchantURLB),
      base::BindOnce(&CartServiceTest::OperationEvaluation,
                     base::Unretained(this), run_loop[4].QuitClosure(), true));
  run_loop[4].Run();

  service_->LoadAllActiveCarts(
      base::BindOnce(&CartServiceTest::GetEvaluationURL, base::Unretained(this),
                     run_loop[5].QuitClosure(), kExpectedA));
  run_loop[5].Run();

  service_->RemoveCart(
      GURL(kMockMerchantURLA),
      base::BindOnce(&CartServiceTest::OperationEvaluation,
                     base::Unretained(this), run_loop[6].QuitClosure(), true));
  run_loop[6].Run();

  service_->LoadAllActiveCarts(
      base::BindOnce(&CartServiceTest::GetEvaluationURL, base::Unretained(this),
                     run_loop[7].QuitClosure(), kEmptyExpected));
  run_loop[7].Run();
}

// Verifies the database is cleared when detected history deletion.
TEST_F(CartServiceTest, TestOnHistoryDeletion) {
  CartDB* cart_db_ = service_->GetDB();
  base::RunLoop run_loop[3];
  cart_db_->AddCart(
      kMockMerchantA, kMockProtoA,
      base::BindOnce(&CartServiceTest::OperationEvaluation,
                     base::Unretained(this), run_loop[0].QuitClosure(), true));
  task_environment_.RunUntilIdle();
  run_loop[0].Run();

  cart_db_->LoadAllCarts(base::BindOnce(&CartServiceTest::GetEvaluationURL,
                                        base::Unretained(this),
                                        run_loop[1].QuitClosure(), kExpectedA));
  task_environment_.RunUntilIdle();
  run_loop[1].Run();

  service_->OnHistoryDeletions(
      HistoryServiceFactory::GetForProfile(profile_.get(),
                                           ServiceAccessType::EXPLICIT_ACCESS),
      history::DeletionInfo(history::DeletionTimeRange::Invalid(), false,
                            history::URLRows(), std::set<GURL>(),
                            std::nullopt));

  cart_db_->LoadAllCarts(
      base::BindOnce(&CartServiceTest::GetEvaluationURL, base::Unretained(this),
                     run_loop[2].QuitClosure(), kEmptyExpected));
  task_environment_.RunUntilIdle();
  run_loop[2].Run();
}

// Tests hiding a single cart and undoing the hide.
TEST_F(CartServiceTest, TestHideCart) {
  CartDB* cart_db_ = service_->GetDB();
  base::RunLoop run_loop[6];
  cart_db_->AddCart(
      kMockMerchantA, kMockProtoA,
      base::BindOnce(&CartServiceTest::OperationEvaluation,
                     base::Unretained(this), run_loop[0].QuitClosure(), true));
  run_loop[0].Run();

  service_->LoadCart(
      kMockMerchantA,
      base::BindOnce(&CartServiceTest::GetEvaluationCartHiddenStatus,
                     base::Unretained(this), run_loop[1].QuitClosure(), false));
  run_loop[1].Run();

  service_->HideCart(
      GURL(kMockMerchantURLA),
      base::BindOnce(&CartServiceTest::OperationEvaluation,
                     base::Unretained(this), run_loop[2].QuitClosure(), true));
  run_loop[2].Run();

  service_->LoadCart(
      kMockMerchantA,
      base::BindOnce(&CartServiceTest::GetEvaluationCartHiddenStatus,
                     base::Unretained(this), run_loop[3].QuitClosure(), true));
  run_loop[3].Run();

  service_->RestoreHiddenCart(
      GURL(kMockMerchantURLA),
      base::BindOnce(&CartServiceTest::OperationEvaluation,
                     base::Unretained(this), run_loop[4].QuitClosure(), true));
  run_loop[4].Run();

  service_->LoadCart(
      kMockMerchantA,
      base::BindOnce(&CartServiceTest::GetEvaluationCartHiddenStatus,
                     base::Unretained(this), run_loop[5].QuitClosure(), false));
  run_loop[5].Run();
}

// Tests removing a single cart and undoing the remove.
TEST_F(CartServiceTest, TestRemoveCart) {
  CartDB* cart_db_ = service_->GetDB();
  base::RunLoop run_loop[6];
  cart_db_->AddCart(
      kMockMerchantA, kMockProtoA,
      base::BindOnce(&CartServiceTest::OperationEvaluation,
                     base::Unretained(this), run_loop[0].QuitClosure(), true));
  run_loop[0].Run();

  service_->LoadCart(
      kMockMerchantA,
      base::BindOnce(&CartServiceTest::GetEvaluationCartRemovedStatus,
                     base::Unretained(this), run_loop[1].QuitClosure(), false));
  run_loop[1].Run();

  service_->RemoveCart(
      GURL(kMockMerchantURLA),
      base::BindOnce(&CartServiceTest::OperationEvaluation,
                     base::Unretained(this), run_loop[2].QuitClosure(), true));
  run_loop[2].Run();

  service_->LoadCart(
      kMockMerchantA,
      base::BindOnce(&CartServiceTest::GetEvaluationCartRemovedStatus,
                     base::Unretained(this), run_loop[3].QuitClosure(), true));
  run_loop[3].Run();

  service_->RestoreRemovedCart(
      GURL(kMockMerchantURLA),
      base::BindOnce(&CartServiceTest::OperationEvaluation,
                     base::Unretained(this), run_loop[4].QuitClosure(), true));
  run_loop[4].Run();

  service_->LoadCart(
      kMockMerchantA,
      base::BindOnce(&CartServiceTest::GetEvaluationCartRemovedStatus,
                     base::Unretained(this), run_loop[5].QuitClosure(), false));
  run_loop[5].Run();
}

// Tests after service shutdown, content of removed cart entries are deleted
// from database except for the removed status data.
TEST_F(CartServiceTest, TestRemovedCartsDeleted) {
  CartDB* cart_db_ = service_->GetDB();
  base::RunLoop run_loop[6];
  cart_db::ChromeCartContentProto merchant_A_proto =
      BuildProto(kMockMerchantA, kMockMerchantURLA);
  merchant_A_proto.add_product_image_urls("https://image1.com");
  cart_db_->AddCart(
      kMockMerchantA, merchant_A_proto,
      base::BindOnce(&CartServiceTest::OperationEvaluation,
                     base::Unretained(this), run_loop[0].QuitClosure(), true));
  run_loop[0].Run();

  service_->RemoveCart(
      GURL(kMockMerchantURLA),
      base::BindOnce(&CartServiceTest::OperationEvaluation,
                     base::Unretained(this), run_loop[1].QuitClosure(), true));
  run_loop[1].Run();

  cart_db_->LoadAllCarts(base::BindOnce(&CartServiceTest::GetEvaluationURL,
                                        base::Unretained(this),
                                        run_loop[2].QuitClosure(), kExpectedA));
  run_loop[2].Run();

  service_->LoadAllActiveCarts(
      base::BindOnce(&CartServiceTest::GetEvaluationURL, base::Unretained(this),
                     run_loop[3].QuitClosure(), kEmptyExpected));
  run_loop[3].Run();

  service_->Shutdown();
  task_environment_.RunUntilIdle();

  // After shut down, cart content is removed and only the removed status is
  // kept.
  cart_db::ChromeCartContentProto empty_proto;
  empty_proto.set_key(kMockMerchantA);
  empty_proto.set_is_removed(true);
  const ShoppingCarts result = {{kMockMerchantA, empty_proto}};
  cart_db_->LoadAllCarts(base::BindOnce(&CartServiceTest::GetEvaluationURL,
                                        base::Unretained(this),
                                        run_loop[4].QuitClosure(), result));
  run_loop[4].Run();
  cart_db_->LoadAllCarts(
      base::BindOnce(&CartServiceTest::GetEvaluationCartRemovedStatus,
                     base::Unretained(this), run_loop[5].QuitClosure(), true));
  run_loop[5].Run();
}

// Tests whether to show the welcome surface is correctly controlled.
TEST_F(CartServiceTest, TestControlShowWelcomeSurface) {
  const int limit = CartService::kWelcomSurfaceShowLimit;
  for (int i = 0; i < limit; i++) {
    EXPECT_EQ(i, profile_->GetPrefs()->GetInteger(
                     prefs::kCartModuleWelcomeSurfaceShownTimes));
    EXPECT_TRUE(service_->ShouldShowWelcomeSurface());
    service_->IncreaseWelcomeSurfaceCounter();
  }
  EXPECT_FALSE(service_->ShouldShowWelcomeSurface());
  EXPECT_EQ(limit, profile_->GetPrefs()->GetInteger(
                       prefs::kCartModuleWelcomeSurfaceShownTimes));
}

// Tests cart data is loaded in the order of timestamp.
TEST_F(CartServiceTest, TestOrderInTimestamp) {
  base::RunLoop run_loop[3];
  double time_now = base::Time::Now().InSecondsFSinceUnixEpoch();
  cart_db::ChromeCartContentProto merchant_A_proto =
      BuildProto(kMockMerchantA, kMockMerchantURLA);
  merchant_A_proto.set_timestamp(time_now);
  cart_db::ChromeCartContentProto merchant_B_proto =
      BuildProto(kMockMerchantB, kMockMerchantURLB);
  merchant_B_proto.set_timestamp(time_now + 1);
  cart_db::ChromeCartContentProto merchant_C_proto =
      BuildProto(kMockMerchantC, kMockMerchantURLC);
  merchant_C_proto.set_timestamp(time_now + 2);
  service_->AddCart(mock_merchant_url_A_, std::nullopt, merchant_A_proto);
  service_->AddCart(mock_merchant_url_B_, std::nullopt, merchant_B_proto);
  service_->AddCart(mock_merchant_url_C_, std::nullopt, merchant_C_proto);
  task_environment_.RunUntilIdle();

  const ShoppingCarts result1 = {{kMockMerchantC, merchant_C_proto},
                                 {kMockMerchantB, merchant_B_proto},
                                 {kMockMerchantA, merchant_A_proto}};
  service_->LoadAllActiveCarts(
      base::BindOnce(&CartServiceTest::GetEvaluationURL, base::Unretained(this),
                     run_loop[0].QuitClosure(), result1));
  run_loop[0].Run();

  merchant_A_proto.set_timestamp(time_now + 3);
  service_->AddCart(mock_merchant_url_A_, std::nullopt, merchant_A_proto);
  task_environment_.RunUntilIdle();
  const ShoppingCarts result2 = {{kMockMerchantA, merchant_A_proto},
                                 {kMockMerchantC, merchant_C_proto},
                                 {kMockMerchantB, merchant_B_proto}};
  service_->LoadAllActiveCarts(
      base::BindOnce(&CartServiceTest::GetEvaluationURL, base::Unretained(this),
                     run_loop[1].QuitClosure(), result2));
  run_loop[1].Run();

  merchant_C_proto.set_timestamp(time_now + 4);
  service_->AddCart(mock_merchant_url_C_, std::nullopt, merchant_C_proto);
  task_environment_.RunUntilIdle();
  const ShoppingCarts result3 = {{kMockMerchantC, merchant_C_proto},
                                 {kMockMerchantA, merchant_A_proto},
                                 {kMockMerchantB, merchant_B_proto}};
  service_->LoadAllActiveCarts(
      base::BindOnce(&CartServiceTest::GetEvaluationURL, base::Unretained(this),
                     run_loop[2].QuitClosure(), result3));
  run_loop[2].Run();
}

// Tests domain to merchant name mapping.
TEST_F(CartServiceTest, TestDomainToNameMapping) {
  EXPECT_EQ("Amazon", getDomainName("amazon.com"));

  EXPECT_EQ("eBay", getDomainName("ebay.com"));

  EXPECT_EQ("", getDomainName("example.com"));
}

// Tests domain to cart URL mapping.
TEST_F(CartServiceTest, TestDomainToCartURLMapping) {
  EXPECT_EQ("https://www.amazon.com/gp/cart/view.html",
            getDomainCartURL("amazon.com"));

  EXPECT_EQ("https://cart.ebay.com/", getDomainCartURL("ebay.com"));

  EXPECT_EQ("", getDomainCartURL("example.com"));
}

// Tests looking up cart URL and merchant name from resources when adding cart.
TEST_F(CartServiceTest, TestLookupCartInfo_FromResource) {
  CartDB* cart_db_ = service_->GetDB();
  const char* amazon_domain = "amazon.com";
  base::RunLoop run_loop[3];
  cart_db::ChromeCartContentProto merchant_A_proto =
      BuildProto(amazon_domain, kMockMerchantURLA);
  service_->AddCart(GURL("https://amazon.com"), std::nullopt, merchant_A_proto);
  task_environment_.RunUntilIdle();
  histogram_tester_.ExpectBucketCount(
      "Commerce.Heuristics.MerchantNameSource",
      CommerceHeuristicsDataMetricsHelper::HeuristicsSource::FROM_RESOURCE, 1);

  merchant_A_proto.set_merchant_cart_url(getDomainCartURL(amazon_domain));
  merchant_A_proto.set_merchant(getDomainName(amazon_domain));
  const ShoppingCarts result1 = {{amazon_domain, merchant_A_proto}};
  cart_db_->LoadCart(
      amazon_domain,
      base::BindOnce(&CartServiceTest::GetEvaluationURL, base::Unretained(this),
                     run_loop[1].QuitClosure(), result1));
  run_loop[1].Run();

  // Use default value when no info can be found in the lookup table.
  service_->DeleteCart(GURL(getDomainCartURL(amazon_domain)), true);
  const char* fake_domain = "fake.com";
  const char* fake_cart_url = "fake.com/cart";
  cart_db::ChromeCartContentProto fake_proto =
      BuildProto(fake_domain, fake_cart_url);
  service_->AddCart(GURL("https://fake.com"), std::nullopt, fake_proto);
  task_environment_.RunUntilIdle();
  histogram_tester_.ExpectBucketCount(
      "Commerce.Heuristics.MerchantNameSource",
      CommerceHeuristicsDataMetricsHelper::HeuristicsSource::MISSING, 1);

  const ShoppingCarts result2 = {{fake_domain, fake_proto}};
  cart_db_->LoadCart(
      fake_domain,
      base::BindOnce(&CartServiceTest::GetEvaluationURL, base::Unretained(this),
                     run_loop[2].QuitClosure(), result2));
  run_loop[2].Run();
}

// Tests looking up merchant name from component when adding cart.
TEST_F(CartServiceTest, TestLookupCartInfo_FromComponent) {
  ASSERT_TRUE(commerce_heuristics::CommerceHeuristicsData::GetInstance()
                  .PopulateDataFromComponent(kCommerceHintHeuristicsJSONData,
                                             "{}", "", ""));
  CartDB* cart_db_ = service_->GetDB();
  base::RunLoop run_loop;
  cart_db::ChromeCartContentProto merchant_proto =
      BuildProto(kMockMerchantA, "https://foo.com/cart");
  service_->AddCart(mock_merchant_url_A_, std::nullopt, merchant_proto);
  task_environment_.RunUntilIdle();
  histogram_tester_.ExpectBucketCount(
      "Commerce.Heuristics.MerchantNameSource",
      CommerceHeuristicsDataMetricsHelper::HeuristicsSource::FROM_COMPONENT, 1);

  merchant_proto.set_merchant("Foo");
  const ShoppingCarts result = {{kMockMerchantA, merchant_proto}};
  cart_db_->LoadCart(
      kMockMerchantA,
      base::BindOnce(&CartServiceTest::GetEvaluationURL, base::Unretained(this),
                     run_loop.QuitClosure(), result));
  run_loop.Run();
}

// Tests the priority of cart URL sources.
TEST_F(CartServiceTest, CartURLPriority) {
  const char amazon_domain[] = "amazon.com";
  const char example_domain[] = "example.com";
  GURL amazon_url = GURL("https://amazon.com");
  GURL amazon_cart = GURL("http://amazon.com/mycart");
  GURL amazon_cart2 = GURL("http://amazon.com/shopping-cart");
  cart_db::ChromeCartContentProto merchant_A_proto =
      BuildProto(amazon_domain, kMockMerchantURLA);

  // The priority of shopping cart URL from highest:
  // - The navigation URL when visiting carts
  // - The existing URL in the cart entry if exist
  // - The look-up table by eTLD+1 domain
  // - The navigation URL

  // * Lowest priority: no overriding.
  service_->AddCart(GURL("https://example.com"), std::nullopt,
                    merchant_A_proto);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(GetCartURL(example_domain), kMockMerchantURLA);

  // * Higher priority: from look up table.
  service_->AddCart(amazon_url, std::nullopt, merchant_A_proto);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(GetCartURL(amazon_domain),
            "https://www.amazon.com/gp/cart/view.html");
  service_->DeleteCart(amazon_cart, true);

  // * Higher priority: from existing entry.
  service_->AddCart(amazon_url, amazon_cart, merchant_A_proto);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(GetCartURL(amazon_domain), amazon_cart.spec());
  service_->AddCart(amazon_url, std::nullopt, merchant_A_proto);
  task_environment_.RunUntilIdle();
  // Lookup table cannot override existing entry.
  EXPECT_EQ(GetCartURL(amazon_domain), amazon_cart.spec());
  service_->DeleteCart(amazon_cart, true);

  // * Highest priority: overriding existing entry.
  service_->AddCart(amazon_url, std::nullopt, merchant_A_proto);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(GetCartURL(amazon_domain),
            "https://www.amazon.com/gp/cart/view.html");
  service_->AddCart(amazon_url, amazon_cart, merchant_A_proto);
  task_environment_.RunUntilIdle();
  // Visiting carts can override existing entry.
  EXPECT_EQ(GetCartURL(amazon_domain), amazon_cart.spec());
  service_->DeleteCart(amazon_cart, true);
  // New visiting carts can override existing entry from earlier visiting carts.
  service_->AddCart(amazon_url, amazon_cart, merchant_A_proto);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(GetCartURL(amazon_domain), amazon_cart.spec());
  service_->AddCart(amazon_url, amazon_cart2, merchant_A_proto);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(GetCartURL(amazon_domain), amazon_cart2.spec());
  service_->AddCart(amazon_url, amazon_cart, merchant_A_proto);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(GetCartURL(amazon_domain), amazon_cart.spec());
}

TEST_F(CartServiceTest, TestCacheUsedDiscounts) {
  EXPECT_FALSE(service_->IsDiscountUsed(kMockMerchantADiscountRuleId));

  cart_db::ChromeCartContentProto cart_with_discount_proto = AddDiscountToProto(
      BuildProto(kMockMerchantA, kMockMerchantURLA), 1,
      kMockMerchantADiscountRuleId, kMockMerchantADiscountsPercentOff,
      kMockMerchantADiscountsRawMerchantOfferId);

  CacheUsedDiscounts(cart_with_discount_proto);
  EXPECT_TRUE(service_->IsDiscountUsed(kMockMerchantADiscountRuleId));
}

TEST_F(CartServiceTest, TestCleanUpDiscounts_RuleBasedDiscount) {
  cart_db::ChromeCartContentProto cart_with_discount_proto = AddDiscountToProto(
      BuildProto(kMockMerchantA, kMockMerchantURLA), 1,
      kMockMerchantADiscountRuleId, kMockMerchantADiscountsPercentOff,
      kMockMerchantADiscountsRawMerchantOfferId);
  const ShoppingCarts has_discount_cart = {
      {kMockMerchantA, cart_with_discount_proto}};
  CartDB* cart_db = service_->GetDB();

  base::RunLoop run_loop[3];
  cart_db->AddCart(
      kMockMerchantA, cart_with_discount_proto,
      base::BindOnce(&CartServiceTest::OperationEvaluation,
                     base::Unretained(this), run_loop[0].QuitClosure(), true));
  run_loop[0].Run();

  cart_db->LoadCart(
      kMockMerchantA,
      base::BindOnce(&CartServiceTest::GetEvaluationDiscount,
                     base::Unretained(this), run_loop[1].QuitClosure(),
                     has_discount_cart));
  run_loop[1].Run();

  CleanUpDiscounts(cart_with_discount_proto);

  cart_db->LoadCart(
      kMockMerchantA,
      base::BindOnce(&CartServiceTest::GetEvaluationEmptyDiscount,
                     base::Unretained(this), run_loop[2].QuitClosure()));
  run_loop[2].Run();
}

TEST_F(CartServiceTest, TestNotCleanUpDiscounts_OtherDiscount) {
  cart_db::ChromeCartContentProto cart_with_discount_proto =
      BuildProto(kMockMerchantA, kMockMerchantURLA);
  cart_with_discount_proto.mutable_discount_info()->set_discount_text(
      "10% off");

  const ShoppingCarts has_discount_cart = {
      {kMockMerchantA, cart_with_discount_proto}};
  CartDB* cart_db = service_->GetDB();

  base::RunLoop run_loop[3];
  cart_db->AddCart(
      kMockMerchantA, cart_with_discount_proto,
      base::BindOnce(&CartServiceTest::OperationEvaluation,
                     base::Unretained(this), run_loop[0].QuitClosure(), true));
  run_loop[0].Run();

  cart_db->LoadCart(
      kMockMerchantA,
      base::BindOnce(&CartServiceTest::GetEvaluationDiscount,
                     base::Unretained(this), run_loop[1].QuitClosure(),
                     has_discount_cart));
  run_loop[1].Run();

  CleanUpDiscounts(cart_with_discount_proto);

  cart_db->LoadCart(
      kMockMerchantA,
      base::BindOnce(&CartServiceTest::GetEvaluationDiscount,
                     base::Unretained(this), run_loop[2].QuitClosure(),
                     has_discount_cart));
  run_loop[2].Run();
}

TEST_F(CartServiceTest, TestUpdateDiscountsTesterByPassCachedRuleId) {
  EXPECT_FALSE(service_->IsDiscountUsed(kMockMerchantADiscountRuleId));

  cart_db::ChromeCartContentProto cart_with_discount_proto = AddDiscountToProto(
      BuildProto(kMockMerchantA, kMockMerchantURLA), 1,
      kMockMerchantADiscountRuleId, kMockMerchantADiscountsPercentOff,
      kMockMerchantADiscountsRawMerchantOfferId);
  const ShoppingCarts has_discount_cart = {
      {kMockMerchantA, cart_with_discount_proto}};

  CacheUsedDiscounts(cart_with_discount_proto);
  EXPECT_TRUE(service_->IsDiscountUsed(kMockMerchantADiscountRuleId));

  base::RunLoop run_loop[2];
  CartDB* cart_db = service_->GetDB();
  service_->UpdateDiscounts(GURL(kMockMerchantURLA), cart_with_discount_proto,
                            kNotATester);
  cart_db->LoadCart(
      kMockMerchantA,
      base::BindOnce(&CartServiceTest::GetEvaluationEmptyDiscount,
                     base::Unretained(this), run_loop[0].QuitClosure()));
  run_loop[0].Run();

  service_->UpdateDiscounts(GURL(kMockMerchantURLA), cart_with_discount_proto,
                            kATester);

  cart_db->LoadCart(
      kMockMerchantA,
      base::BindOnce(&CartServiceTest::GetEvaluationDiscount,
                     base::Unretained(this), run_loop[1].QuitClosure(),
                     has_discount_cart));
  run_loop[1].Run();
}

class CartServiceFakeDataTest : public CartServiceTest {
 public:
  // Features need to be initialized before CartServiceTest::SetUp runs, in
  // order to avoid tsan data race error on FeatureList.
  CartServiceFakeDataTest() {
    features_.InitAndEnableFeatureWithParameters(
        ntp_features::kNtpChromeCartModule,
        {{"NtpChromeCartModuleDataParam", "fake"}});
  }
};

TEST_F(CartServiceFakeDataTest, TestFakeData) {
  base::RunLoop run_loop[2];
  service_->LoadCartsWithFakeData(
      base::BindOnce(&CartServiceTest::GetEvaluationFakeDataDB,
                     base::Unretained(this), run_loop[0].QuitClosure()));
  run_loop[0].Run();

  service_->Shutdown();

  service_->GetDB()->LoadAllCarts(
      base::BindOnce(&CartServiceTest::GetEvaluationURL, base::Unretained(this),
                     run_loop[1].QuitClosure(), kEmptyExpected));
  run_loop[1].Run();
}

// Tests expired entries are deleted when data is loaded.
TEST_F(CartServiceTest, TestExpiredDataDeleted) {
  base::RunLoop run_loop[6];
  cart_db::ChromeCartContentProto merchant_proto =
      BuildProto(kMockMerchantA, kMockMerchantURLA);
  const ShoppingCarts result = {{kMockMerchantA, merchant_proto}};

  merchant_proto.set_timestamp(
      (base::Time::Now() -
       base::Days(CartService::kCartExpirationTimeInDays + 2))
          .InSecondsFSinceUnixEpoch());
  service_->AddCart(mock_merchant_url_A_, std::nullopt, merchant_proto);
  task_environment_.RunUntilIdle();

  // The expired entry is deleted in load results.
  service_->LoadAllActiveCarts(
      base::BindOnce(&CartServiceTest::GetEvaluationURL, base::Unretained(this),
                     run_loop[0].QuitClosure(), kEmptyExpected));
  run_loop[0].Run();

  // The expired entry is deleted in database.
  service_->LoadCart(
      kMockMerchantA,
      base::BindOnce(&CartServiceTest::GetEvaluationURL, base::Unretained(this),
                     run_loop[1].QuitClosure(), kEmptyExpected));
  run_loop[1].Run();

  // If the cart is removed, the expired entry is deleted in load results but is
  // kept in database.
  merchant_proto.set_is_removed(true);
  service_->AddCart(mock_merchant_url_A_, std::nullopt, merchant_proto);
  task_environment_.RunUntilIdle();

  service_->LoadAllActiveCarts(
      base::BindOnce(&CartServiceTest::GetEvaluationURL, base::Unretained(this),
                     run_loop[2].QuitClosure(), kEmptyExpected));
  run_loop[2].Run();

  service_->LoadCart(
      kMockMerchantA,
      base::BindOnce(&CartServiceTest::GetEvaluationURL, base::Unretained(this),
                     run_loop[3].QuitClosure(), result));
  run_loop[3].Run();

  merchant_proto.set_timestamp(
      (base::Time::Now() -
       base::Days(CartService::kCartExpirationTimeInDays - 2))
          .InSecondsFSinceUnixEpoch());
  merchant_proto.set_is_removed(false);
  service_->GetDB()->AddCart(
      kMockMerchantA, merchant_proto,
      base::BindOnce(&CartServiceTest::OperationEvaluation,
                     base::Unretained(this), run_loop[4].QuitClosure(), true));
  run_loop[4].Run();

  service_->LoadCart(
      kMockMerchantA,
      base::BindOnce(&CartServiceTest::GetEvaluationCartRemovedStatus,
                     base::Unretained(this), run_loop[5].QuitClosure(), false));
  run_loop[5].Run();
}

// Tests cart-related actions would reshow hidden module.
TEST_F(CartServiceTest, TestHiddenFlipedByCartAction) {
  base::RunLoop run_loop[3];
  cart_db::ChromeCartContentProto merchant_proto =
      BuildProto(kMockMerchantA, kMockMerchantURLA);
  const ShoppingCarts result = {{kMockMerchantA, merchant_proto}};
  service_->AddCart(mock_merchant_url_A_, std::nullopt, merchant_proto);
  task_environment_.RunUntilIdle();
  service_->LoadAllActiveCarts(
      base::BindOnce(&CartServiceTest::GetEvaluationURL, base::Unretained(this),
                     run_loop[0].QuitClosure(), result));
  run_loop[0].Run();

  service_->Hide();
  ASSERT_TRUE(service_->IsHidden());
  service_->LoadAllActiveCarts(
      base::BindOnce(&CartServiceTest::GetEvaluationURL, base::Unretained(this),
                     run_loop[1].QuitClosure(), kEmptyExpected));
  run_loop[1].Run();

  service_->AddCart(mock_merchant_url_A_, std::nullopt, merchant_proto);
  task_environment_.RunUntilIdle();
  ASSERT_FALSE(service_->IsHidden());
  service_->LoadAllActiveCarts(
      base::BindOnce(&CartServiceTest::GetEvaluationURL, base::Unretained(this),
                     run_loop[2].QuitClosure(), result));
  run_loop[2].Run();
}

// Tests discount consent will never show in module without feature param.
TEST_F(CartServiceTest, TestNoShowConsentWithoutFeature) {
  base::RunLoop run_loop;
  for (int i = 0; i < CartService::kWelcomSurfaceShowLimit + 1; i++) {
    service_->IncreaseWelcomeSurfaceCounter();
  }
  ASSERT_FALSE(service_->ShouldShowWelcomeSurface());
  service_->ShouldShowDiscountConsent(
      base::BindOnce(&CartServiceTest::GetEvaluationBoolResult,
                     base::Unretained(this), run_loop.QuitClosure(), false));
  run_loop.Run();
}

// Tests acknowledging discount consent is reflected in profile pref.
TEST_F(CartServiceTest, TestAcknowledgeDiscountConsent) {
  ASSERT_FALSE(profile_->GetPrefs()->GetBoolean(prefs::kCartDiscountEnabled));
  ASSERT_FALSE(
      profile_->GetPrefs()->GetBoolean(prefs::kCartDiscountAcknowledged));

  service_->AcknowledgeDiscountConsent(true);
  ASSERT_TRUE(profile_->GetPrefs()->GetBoolean(prefs::kCartDiscountEnabled));
  ASSERT_TRUE(
      profile_->GetPrefs()->GetBoolean(prefs::kCartDiscountAcknowledged));

  profile_->GetPrefs()->SetBoolean(prefs::kCartDiscountAcknowledged, false);
  service_->AcknowledgeDiscountConsent(false);
  ASSERT_FALSE(profile_->GetPrefs()->GetBoolean(prefs::kCartDiscountEnabled));
  ASSERT_TRUE(
      profile_->GetPrefs()->GetBoolean(prefs::kCartDiscountAcknowledged));
}

// Tests HasActiveCartForURL API correctly checks cart existence.
TEST_F(CartServiceTest, TestHasActiveCartForURL) {
  base::RunLoop run_loop[4];
  const GURL url_with_cart_A = GURL("https://www.foo.com/A");
  const GURL url_with_cart_B = GURL("https://www.foo.com/B");
  const GURL url_without_cart = GURL("https://www.bar.com/A");
  cart_db::ChromeCartContentProto merchant_proto =
      BuildProto(kMockMerchantA, "https://www.foo.com/A");

  service_->AddCart(url_with_cart_A, std::nullopt, merchant_proto);
  task_environment_.RunUntilIdle();

  service_->HasActiveCartForURL(
      url_with_cart_A,
      base::BindOnce(&CartServiceTest::GetEvaluationBoolResult,
                     base::Unretained(this), run_loop[0].QuitClosure(), true));
  run_loop[0].Run();
  service_->HasActiveCartForURL(
      url_with_cart_B,
      base::BindOnce(&CartServiceTest::GetEvaluationBoolResult,
                     base::Unretained(this), run_loop[1].QuitClosure(), true));
  run_loop[1].Run();
  service_->HasActiveCartForURL(
      url_without_cart,
      base::BindOnce(&CartServiceTest::GetEvaluationBoolResult,
                     base::Unretained(this), run_loop[2].QuitClosure(), false));
  run_loop[2].Run();

  // Overwrite the cart entry for current domain with an expired cart.
  merchant_proto.set_timestamp(
      (base::Time::Now() -
       base::Days(CartService::kCartExpirationTimeInDays + 2))
          .InSecondsFSinceUnixEpoch());
  service_->AddCart(url_with_cart_A, std::nullopt, merchant_proto);
  task_environment_.RunUntilIdle();

  service_->HasActiveCartForURL(
      url_with_cart_A,
      base::BindOnce(&CartServiceTest::GetEvaluationBoolResult,
                     base::Unretained(this), run_loop[3].QuitClosure(), false));
  run_loop[3].Run();
}

TEST_F(CartServiceTest, TestIsCartEnabled) {
  const std::string cart_key = "chrome_cart";
  ScopedListPrefUpdate update(profile_->GetPrefs(), prefs::kNtpDisabledModules);
  base::Value::List& disabled_list = update.Get();

  disabled_list.Append(base::Value(cart_key));

  ASSERT_TRUE(base::Contains(disabled_list, base::Value(cart_key)));
  ASSERT_FALSE(service_->IsCartEnabled());

  disabled_list.EraseValue(base::Value(cart_key));

  ASSERT_FALSE(base::Contains(disabled_list, base::Value(cart_key)));
  ASSERT_TRUE(service_->IsCartEnabled());
}

class CartServiceNoDiscountTest : public CartServiceTest {
 public:
  // Features need to be initialized before CartServiceTest::SetUp runs, in
  // order to avoid tsan data race error on FeatureList.
  CartServiceNoDiscountTest() {
    features_.InitAndEnableFeatureWithParameters(
        ntp_features::kNtpChromeCartModule,
        {{ntp_features::kNtpChromeCartModuleAbandonedCartDiscountParam,
          "false"}});
  }

  void TearDown() override {
    // Set the feature to default disabled state after test.
    profile_->GetPrefs()->SetBoolean(prefs::kCartDiscountEnabled, false);
  }
};

// Tests discount is disabled without feature param.
TEST_F(CartServiceNoDiscountTest, TestDiscountDisabledWithoutFeature) {
  profile_->GetPrefs()->SetBoolean(prefs::kCartDiscountEnabled, true);
  ASSERT_FALSE(service_->IsCartDiscountEnabled());
}

class MockCartDiscountLinkFetcher : public CartDiscountLinkFetcher {
 public:
  MOCK_METHOD(
      void,
      Fetch,
      (std::unique_ptr<network::PendingSharedURLLoaderFactory> pending_factory,
       cart_db::ChromeCartContentProto cart_content_proto,
       CartDiscountLinkFetcherCallback callback),
      (override));

  void SetDiscountURL(const GURL& discount_url) {
    ON_CALL(*this, Fetch)
        .WillByDefault(
            [discount_url](
                std::unique_ptr<network::PendingSharedURLLoaderFactory>
                    pending_factory,
                cart_db::ChromeCartContentProto cart_content_proto,
                CartDiscountLinkFetcherCallback callback) {
              return std::move(callback).Run(discount_url);
            });
  }
};

class CartServiceDiscountTest : public CartServiceTest {
 public:
  // Features need to be initialized before CartServiceTest::SetUp runs, in
  // order to avoid tsan data race error on FeatureList.
  CartServiceDiscountTest() {
    std::vector<base::test::FeatureRefAndParams> enabled_features;
    base::FieldTrialParams cart_params, coupon_params, code_based_rbd_param;
    cart_params[ntp_features::kNtpChromeCartModuleAbandonedCartDiscountParam] =
        "true";

    enabled_features.emplace_back(ntp_features::kNtpChromeCartModule,
                                  cart_params);
    enabled_features.emplace_back(commerce::kRetailCoupons, coupon_params);

    code_based_rbd_param[commerce::kCodeBasedRuleDiscountParam] = "true";
    enabled_features.emplace_back(commerce::kCodeBasedRBD,
                                  code_based_rbd_param);

    features_.InitWithFeaturesAndParameters(enabled_features,
                                            /*disabled_features*/ {});
  }

  void SetUp() override {
    CartServiceTest::SetUp();

    // Add a partner merchant cart.
    service_->AddCart(mock_merchant_url_A_, std::nullopt, kMockProtoA);
    task_environment_.RunUntilIdle();
    // The feature is enabled for this test class.
    profile_->GetPrefs()->SetBoolean(prefs::kCartDiscountEnabled, true);

    auto& data = commerce_heuristics::CommerceHeuristicsData::GetInstance();
    ASSERT_TRUE(data.PopulateDataFromComponent("{}", R"###(
        {
          "rule_discount_partner_merchant_regex": "(foo.com)",
          "coupon_discount_partner_merchant_regex": "(bar.com)"
        }
    )###",
                                               "", ""));
  }

  void TearDown() override {
    // Set the feature to default disabled state after test.
    profile_->GetPrefs()->SetBoolean(prefs::kCartDiscountEnabled, false);
  }

  void SetCartDiscountURLForTesting(const GURL& discount_url,
                                    bool expect_call) {
    std::unique_ptr<MockCartDiscountLinkFetcher> mock_fetcher =
        std::make_unique<MockCartDiscountLinkFetcher>();
    mock_fetcher->SetDiscountURL(discount_url);
    if (expect_call) {
      EXPECT_CALL(*mock_fetcher, Fetch);
    }
    service_->SetCartDiscountLinkFetcherForTesting(std::move(mock_fetcher));
  }
};

// Tests discount consent should not show when welcome surface is still showing.
TEST_F(CartServiceDiscountTest, TestNoConsentWhenWelcomeSurface) {
  base::RunLoop run_loop;
  profile_->GetPrefs()->SetBoolean(prefs::kCartDiscountEnabled, true);

  ASSERT_TRUE(service_->ShouldShowWelcomeSurface());
  service_->ShouldShowDiscountConsent(
      base::BindOnce(&CartServiceTest::GetEvaluationBoolResult,
                     base::Unretained(this), run_loop.QuitClosure(), false));
  run_loop.Run();
}

// Tests discount consent visibility aligns with profile prefs.
TEST_F(CartServiceDiscountTest, TestReadConsentFromPrefs) {
  base::RunLoop run_loop[2];
  for (int i = 0; i < CartService::kWelcomSurfaceShowLimit + 1; i++) {
    service_->IncreaseWelcomeSurfaceCounter();
  }
  ASSERT_FALSE(
      profile_->GetPrefs()->GetBoolean(prefs::kCartDiscountAcknowledged));
  ASSERT_FALSE(service_->ShouldShowWelcomeSurface());
  service_->ShouldShowDiscountConsent(
      base::BindOnce(&CartServiceTest::GetEvaluationBoolResult,
                     base::Unretained(this), run_loop[0].QuitClosure(), true));
  run_loop[0].Run();

  profile_->GetPrefs()->SetBoolean(prefs::kCartDiscountAcknowledged, true);

  ASSERT_TRUE(
      profile_->GetPrefs()->GetBoolean(prefs::kCartDiscountAcknowledged));
  ASSERT_FALSE(service_->ShouldShowWelcomeSurface());
  service_->ShouldShowDiscountConsent(
      base::BindOnce(&CartServiceTest::GetEvaluationBoolResult,
                     base::Unretained(this), run_loop[1].QuitClosure(), false));
  run_loop[1].Run();
}

// Tests discount consent doesn't show when there is no partner merchant cart.
TEST_F(CartServiceDiscountTest, TestNoConsentWithoutPartnerCart) {
  base::RunLoop run_loop[3];
  for (int i = 0; i < CartService::kWelcomSurfaceShowLimit + 1; i++) {
    service_->IncreaseWelcomeSurfaceCounter();
  }
  ASSERT_FALSE(service_->ShouldShowWelcomeSurface());
  service_->ShouldShowDiscountConsent(
      base::BindOnce(&CartServiceTest::GetEvaluationBoolResult,
                     base::Unretained(this), run_loop[0].QuitClosure(), true));
  run_loop[0].Run();

  service_->DeleteCart(GURL(kMockMerchantURLA), true);
  task_environment_.RunUntilIdle();

  service_->ShouldShowDiscountConsent(
      base::BindOnce(&CartServiceTest::GetEvaluationBoolResult,
                     base::Unretained(this), run_loop[1].QuitClosure(), false));
  run_loop[1].Run();

  service_->AddCart(mock_merchant_url_B_, std::nullopt, kMockProtoB);
  task_environment_.RunUntilIdle();

  service_->ShouldShowDiscountConsent(
      base::BindOnce(&CartServiceTest::GetEvaluationBoolResult,
                     base::Unretained(this), run_loop[2].QuitClosure(), true));
  run_loop[2].Run();
}

// Tests updating whether rule-based discount is enabled in profile prefs.
TEST_F(CartServiceDiscountTest, TestSetCartDiscountEnabled) {
  ASSERT_TRUE(profile_->GetPrefs()->GetBoolean(prefs::kCartDiscountEnabled));
  service_->SetCartDiscountEnabled(false);
  ASSERT_FALSE(profile_->GetPrefs()->GetBoolean(prefs::kCartDiscountEnabled));
  service_->SetCartDiscountEnabled(true);
  ASSERT_TRUE(profile_->GetPrefs()->GetBoolean(prefs::kCartDiscountEnabled));
}

// Tests no fetching for discount URL if the cart is not from a partner
// merchant.
TEST_F(CartServiceDiscountTest, TestNoFetchForNonPartner) {
  base::RunLoop run_loop[2];
  SetCartDiscountURLForTesting(GURL("https://www.discount.com"), false);
  cart_db::ChromeCartContentProto cart_proto =
      BuildProto(kMockMerchantC, kMockMerchantURLC);
  service_->GetDB()->AddCart(
      kMockMerchantC, cart_proto,
      base::BindOnce(&CartServiceTest::OperationEvaluation,
                     base::Unretained(this), run_loop[0].QuitClosure(), true));
  run_loop[0].Run();

  GURL default_cart_url(kMockMerchantURLWithCartUtmC);
  service_->GetDiscountURL(
      default_cart_url,
      base::BindOnce(&CartServiceTest::GetEvaluationDiscountURL,
                     base::Unretained(this), run_loop[1].QuitClosure(),
                     default_cart_url));
  run_loop[1].Run();
  histogram_tester_.ExpectBucketCount("NewTabPage.Carts.ClickCart.HasDiscount",
                                      false, 1);
  histogram_tester_.ExpectBucketCount("NewTabPage.Carts.ClickCart.HasDiscount",
                                      true, 0);
}

// Tests no fetching for discount URL if the cart doesn't have discount info.
TEST_F(CartServiceDiscountTest, TestNoFetchWhenNoDiscount) {
  base::RunLoop run_loop[2];
  SetCartDiscountURLForTesting(GURL("https://www.discount.com"), false);
  service_->LoadCart(
      kMockMerchantA,
      base::BindOnce(&CartServiceTest::GetEvaluationEmptyDiscount,
                     base::Unretained(this), run_loop[0].QuitClosure()));
  run_loop[0].Run();

  GURL default_cart_url(kMockMerchantURLWithDiscountUtmA);
  service_->GetDiscountURL(
      default_cart_url,
      base::BindOnce(&CartServiceTest::GetEvaluationDiscountURL,
                     base::Unretained(this), run_loop[1].QuitClosure(),
                     default_cart_url));
  run_loop[1].Run();
  histogram_tester_.ExpectBucketCount("NewTabPage.Carts.ClickCart.HasDiscount",
                                      false, 1);
  histogram_tester_.ExpectBucketCount("NewTabPage.Carts.ClickCart.HasDiscount",
                                      true, 0);
}

// Tests no fetching for discount URL if the feature is disabled.
TEST_F(CartServiceDiscountTest, TestNoFetchWhenFeatureDisabled) {
  base::RunLoop run_loop[2];
  const double timestamp = 1;
  GURL discount_url("https://www.discount.com");
  SetCartDiscountURLForTesting(discount_url, false);
  profile_->GetPrefs()->SetBoolean(prefs::kCartDiscountEnabled, false);
  cart_db::ChromeCartContentProto cart_proto = AddDiscountToProto(
      BuildProto(kMockMerchantA, kMockMerchantURLA), timestamp,
      kMockMerchantADiscountRuleId, kMockMerchantADiscountsPercentOff,
      kMockMerchantADiscountsRawMerchantOfferId);
  service_->GetDB()->AddCart(
      kMockMerchantA, cart_proto,
      base::BindOnce(&CartServiceTest::OperationEvaluation,
                     base::Unretained(this), run_loop[0].QuitClosure(), true));
  run_loop[0].Run();

  GURL default_cart_url(kMockMerchantURLWithNoDiscountUtmA);
  service_->GetDiscountURL(
      default_cart_url,
      base::BindOnce(&CartServiceTest::GetEvaluationDiscountURL,
                     base::Unretained(this), run_loop[1].QuitClosure(),
                     default_cart_url));
  run_loop[1].Run();
  histogram_tester_.ExpectBucketCount("NewTabPage.Carts.ClickCart.HasDiscount",
                                      false, 1);
  histogram_tester_.ExpectBucketCount("NewTabPage.Carts.ClickCart.HasDiscount",
                                      true, 0);
}

// Tests no fetching discounted URL for coupon discount.
TEST_F(CartServiceDiscountTest, TestNoDiscountedURLFetchForCouponDiscount) {
  base::RunLoop run_loop[2];
  const double timestamp = 1;
  GURL discount_url("https://www.discount.com");
  SetCartDiscountURLForTesting(discount_url, /*expect_call=*/false);
  cart_db::ChromeCartContentProto cart_proto = AddCouponDiscountToProto(
      BuildProto(kMockMerchantA, kMockMerchantURLA), timestamp,
      /*discount_text=*/"10% off");
  service_->GetDB()->AddCart(
      kMockMerchantA, cart_proto,
      base::BindOnce(&CartServiceTest::OperationEvaluation,
                     base::Unretained(this), run_loop[0].QuitClosure(), true));
  run_loop[0].Run();

  GURL default_cart_url(kMockMerchantURLWithDiscountUtmA);
  service_->GetDiscountURL(
      default_cart_url,
      base::BindOnce(&CartServiceTest::GetEvaluationDiscountURL,
                     base::Unretained(this), run_loop[1].QuitClosure(),
                     default_cart_url));
  run_loop[1].Run();
  histogram_tester_.ExpectBucketCount("NewTabPage.Carts.ClickCart.HasDiscount",
                                      false, 0);
  histogram_tester_.ExpectBucketCount("NewTabPage.Carts.ClickCart.HasDiscount",
                                      true, 1);
}

// Clicking code-based RBD discount results in coupon consumption, and no
// fetching.
TEST_F(CartServiceDiscountTest,
       TestNoDiscountedURLFetchForCodeBasedRuleDiscount) {
  base::RunLoop run_loop[3];
  const double timestamp = 1;
  GURL discount_url("https://www.discount.com");
  SetCartDiscountURLForTesting(discount_url, /*expect_call=*/false);
  cart_db::ChromeCartContentProto cart_proto = AddCouponDiscountToProto(
      BuildProto(kMockMerchantA, kMockMerchantURLWithDiscountUtmA), timestamp,
      /*discount_text=*/"10% off", kMockMerchantADiscountPromoId);
  service_->GetDB()->AddCart(
      kMockMerchantA, cart_proto,
      base::BindOnce(&CartServiceTest::OperationEvaluation,
                     base::Unretained(this), run_loop[0].QuitClosure(), true));
  run_loop[0].Run();

  const ShoppingCarts expected = {{kMockMerchantA, cart_proto}};

  service_->GetDB()->LoadCart(
      kMockMerchantA, base::BindOnce(&CartServiceTest::GetEvaluationDiscount,
                                     base::Unretained(this),
                                     run_loop[1].QuitClosure(), expected));
  run_loop[1].Run();

  GURL default_cart_url(kMockMerchantURLWithDiscountUtmA);
  service_->GetDiscountURL(
      default_cart_url,
      base::BindOnce(&CartServiceTest::GetEvaluationDiscountURL,
                     base::Unretained(this), run_loop[2].QuitClosure(),
                     default_cart_url));
  run_loop[2].Run();
  histogram_tester_.ExpectBucketCount("NewTabPage.Carts.ClickCart.HasDiscount",
                                      false, 0);
  histogram_tester_.ExpectBucketCount("NewTabPage.Carts.ClickCart.HasDiscount",
                                      true, 1);
}

// Tests CartService returning fetched discount URL.
TEST_F(CartServiceDiscountTest, TestReturnDiscountURL) {
  base::RunLoop run_loop[4];
  const double timestamp = 1;
  GURL discount_url(kMockMerchantURLWithDiscountUtmA);
  SetCartDiscountURLForTesting(discount_url, true);
  EXPECT_FALSE(service_->IsDiscountUsed(kMockMerchantADiscountRuleId));
  cart_db::ChromeCartContentProto cart_proto = AddDiscountToProto(
      BuildProto(kMockMerchantA, kMockMerchantURLA), timestamp,
      kMockMerchantADiscountRuleId, kMockMerchantADiscountsPercentOff,
      kMockMerchantADiscountsRawMerchantOfferId);
  const ShoppingCarts has_discount_cart = {{kMockMerchantA, cart_proto}};
  service_->GetDB()->AddCart(
      kMockMerchantA, cart_proto,
      base::BindOnce(&CartServiceTest::OperationEvaluation,
                     base::Unretained(this), run_loop[0].QuitClosure(), true));
  run_loop[0].Run();
  service_->LoadCart(
      kMockMerchantA,
      base::BindOnce(&CartServiceTest::GetEvaluationDiscount,
                     base::Unretained(this), run_loop[1].QuitClosure(),
                     has_discount_cart));
  run_loop[1].Run();

  service_->GetDiscountURL(
      GURL(kMockMerchantURLA),
      base::BindOnce(&CartServiceTest::GetEvaluationDiscountURL,
                     base::Unretained(this), run_loop[2].QuitClosure(),
                     discount_url));
  run_loop[2].Run();

  EXPECT_TRUE(service_->IsDiscountUsed(kMockMerchantADiscountRuleId));
  service_->LoadCart(
      kMockMerchantA,
      base::BindOnce(&CartServiceTest::GetEvaluationEmptyDiscount,
                     base::Unretained(this), run_loop[3].QuitClosure()));
  run_loop[3].Run();
  histogram_tester_.ExpectBucketCount("NewTabPage.Carts.ClickCart.HasDiscount",
                                      false, 0);
  histogram_tester_.ExpectBucketCount("NewTabPage.Carts.ClickCart.HasDiscount",
                                      true, 1);
}

// Tests CartService returning original cart URL as a fallback if the fetch
// response is invalid.
TEST_F(CartServiceDiscountTest, TestFetchInvalidFallback) {
  base::RunLoop run_loop[2];
  const double timestamp = 1;
  SetCartDiscountURLForTesting(GURL("error"), true);
  cart_db::ChromeCartContentProto cart_proto = AddDiscountToProto(
      BuildProto(kMockMerchantA, kMockMerchantURLA), timestamp,
      kMockMerchantADiscountRuleId, kMockMerchantADiscountsPercentOff,
      kMockMerchantADiscountsRawMerchantOfferId);
  service_->GetDB()->AddCart(
      kMockMerchantA, cart_proto,
      base::BindOnce(&CartServiceTest::OperationEvaluation,
                     base::Unretained(this), run_loop[0].QuitClosure(), true));
  run_loop[0].Run();

  GURL default_cart_url(kMockMerchantURLWithDiscountUtmA);
  service_->GetDiscountURL(
      default_cart_url,
      base::BindOnce(&CartServiceTest::GetEvaluationDiscountURL,
                     base::Unretained(this), run_loop[1].QuitClosure(),
                     default_cart_url));
  run_loop[1].Run();
}

// Tests whether discount consent has shown is correctly recorded.
TEST_F(CartServiceDiscountTest, TestSetDiscountConsentShown) {
  base::RunLoop run_loop[2];
  for (int i = 0; i < CartService::kWelcomSurfaceShowLimit + 1; i++) {
    service_->IncreaseWelcomeSurfaceCounter();
  }
  ASSERT_FALSE(service_->ShouldShowWelcomeSurface());
  ASSERT_FALSE(
      profile_->GetPrefs()->GetBoolean(prefs::kCartDiscountConsentShown));

  service_->ShouldShowDiscountConsent(
      base::BindOnce(&CartServiceTest::GetEvaluationBoolResult,
                     base::Unretained(this), run_loop[0].QuitClosure(), true));
  run_loop[0].Run();
  ASSERT_TRUE(
      profile_->GetPrefs()->GetBoolean(prefs::kCartDiscountConsentShown));

  service_->DeleteCart(GURL(kMockMerchantURLA), true);
  service_->ShouldShowDiscountConsent(
      base::BindOnce(&CartServiceTest::GetEvaluationBoolResult,
                     base::Unretained(this), run_loop[1].QuitClosure(), false));
  run_loop[1].Run();
  ASSERT_TRUE(
      profile_->GetPrefs()->GetBoolean(prefs::kCartDiscountConsentShown));
}

TEST_F(CartServiceDiscountTest, TestRecordDiscountConsentStatus_NotRecord) {
  base::RunLoop run_loop[2];
  // Simulate that the welcome surface is not showing, the discount feature is
  // disabled and there is no partner merchant carts.
  profile_->GetPrefs()->SetInteger(prefs::kCartModuleWelcomeSurfaceShownTimes,
                                   CartService::kWelcomSurfaceShowLimit);
  profile_->GetPrefs()->SetBoolean(prefs::kCartDiscountEnabled, false);
  service_->GetDB()->DeleteAllCarts(
      base::BindOnce(&CartServiceTest::OperationEvaluation,
                     base::Unretained(this), run_loop[0].QuitClosure(), true));
  run_loop[0].Run();

  // Don't record when there is no abandoned cart and cart module is not
  // showing.
  service_->ShouldShowDiscountConsent(
      base::BindOnce(&CartServiceTest::GetEvaluationBoolResult,
                     base::Unretained(this), run_loop[1].QuitClosure(), false));
  run_loop[1].Run();
  histogram_tester_.ExpectTotalCount(
      "NewTabPage.Carts.DiscountConsentStatusAtLoad", 0);
}

TEST_F(CartServiceDiscountTest, TestRecordDiscountConsentStatus_NeverShown) {
  base::RunLoop run_loop[2];
  // Simulate that the welcome surface is not showing, the discount feature is
  // disabled and there is no partner merchant carts.
  profile_->GetPrefs()->SetInteger(prefs::kCartModuleWelcomeSurfaceShownTimes,
                                   CartService::kWelcomSurfaceShowLimit);
  profile_->GetPrefs()->SetBoolean(prefs::kCartDiscountEnabled, false);
  service_->GetDB()->DeleteAllCarts(
      base::BindOnce(&CartServiceTest::OperationEvaluation,
                     base::Unretained(this), run_loop[0].QuitClosure(), true));
  run_loop[0].Run();

  // Add a non-partner-merchant cart.
  service_->AddCart(mock_merchant_url_C_, std::nullopt, kMockProtoC);
  task_environment_.RunUntilIdle();

  service_->ShouldShowDiscountConsent(
      base::BindOnce(&CartServiceTest::GetEvaluationBoolResult,
                     base::Unretained(this), run_loop[1].QuitClosure(), false));
  run_loop[1].Run();
  histogram_tester_.ExpectBucketCount(
      "NewTabPage.Carts.DiscountConsentStatusAtLoad",
      CartDiscountMetricCollector::DiscountConsentStatus::NEVER_SHOWN, 1);
}

TEST_F(CartServiceDiscountTest, TestRecordDiscountConsentStatus_NoShow) {
  base::RunLoop run_loop[2];
  // Simulate that the welcome surface is not showing, the discount feature is
  // disabled and there is no partner merchant carts.
  profile_->GetPrefs()->SetInteger(prefs::kCartModuleWelcomeSurfaceShownTimes,
                                   CartService::kWelcomSurfaceShowLimit);
  profile_->GetPrefs()->SetBoolean(prefs::kCartDiscountEnabled, false);
  service_->GetDB()->DeleteAllCarts(
      base::BindOnce(&CartServiceTest::OperationEvaluation,
                     base::Unretained(this), run_loop[0].QuitClosure(), true));
  run_loop[0].Run();

  // Add a non-partner-merchant cart, and simulate that the consent has shown
  // before but the user has never acted on it.
  profile_->GetPrefs()->SetBoolean(prefs::kCartDiscountConsentShown, true);
  service_->AddCart(mock_merchant_url_C_, std::nullopt, kMockProtoC);
  task_environment_.RunUntilIdle();

  service_->ShouldShowDiscountConsent(
      base::BindOnce(&CartServiceTest::GetEvaluationBoolResult,
                     base::Unretained(this), run_loop[1].QuitClosure(), false));
  run_loop[1].Run();
  histogram_tester_.ExpectBucketCount(
      "NewTabPage.Carts.DiscountConsentStatusAtLoad",
      CartDiscountMetricCollector::DiscountConsentStatus::NO_SHOW, 1);
}

TEST_F(CartServiceDiscountTest, TestRecordDiscountConsentStatus_Ignored) {
  base::RunLoop run_loop[2];
  // Simulate that the welcome surface is not showing, the discount feature is
  // disabled and there is no partner merchant carts.
  profile_->GetPrefs()->SetInteger(prefs::kCartModuleWelcomeSurfaceShownTimes,
                                   CartService::kWelcomSurfaceShowLimit);
  profile_->GetPrefs()->SetBoolean(prefs::kCartDiscountEnabled, false);
  service_->GetDB()->DeleteAllCarts(
      base::BindOnce(&CartServiceTest::OperationEvaluation,
                     base::Unretained(this), run_loop[0].QuitClosure(), true));
  run_loop[0].Run();

  // Add a partner-merchant cart.
  service_->AddCart(mock_merchant_url_A_, std::nullopt, kMockProtoA);
  task_environment_.RunUntilIdle();

  service_->ShouldShowDiscountConsent(
      base::BindOnce(&CartServiceTest::GetEvaluationBoolResult,
                     base::Unretained(this), run_loop[1].QuitClosure(), true));
  run_loop[1].Run();
  histogram_tester_.ExpectBucketCount(
      "NewTabPage.Carts.DiscountConsentStatusAtLoad",
      CartDiscountMetricCollector::DiscountConsentStatus::IGNORED, 1);
}

TEST_F(CartServiceDiscountTest, TestRecordDiscountConsentStatus_Declined) {
  base::RunLoop run_loop[2];
  // Simulate that the welcome surface is not showing, the discount feature is
  // disabled and there is no partner merchant carts.
  profile_->GetPrefs()->SetInteger(prefs::kCartModuleWelcomeSurfaceShownTimes,
                                   CartService::kWelcomSurfaceShowLimit);
  service_->GetDB()->DeleteAllCarts(
      base::BindOnce(&CartServiceTest::OperationEvaluation,
                     base::Unretained(this), run_loop[0].QuitClosure(), true));
  run_loop[0].Run();

  // Add a non-partner-merchant cart, and simulate that user has rejected the
  // consent.
  service_->AddCart(mock_merchant_url_A_, std::nullopt, kMockProtoA);
  task_environment_.RunUntilIdle();
  profile_->GetPrefs()->SetBoolean(prefs::kCartDiscountAcknowledged, true);
  profile_->GetPrefs()->SetBoolean(prefs::kCartDiscountEnabled, false);

  service_->ShouldShowDiscountConsent(
      base::BindOnce(&CartServiceTest::GetEvaluationBoolResult,
                     base::Unretained(this), run_loop[1].QuitClosure(), false));
  run_loop[1].Run();
  histogram_tester_.ExpectBucketCount(
      "NewTabPage.Carts.DiscountConsentStatusAtLoad",
      CartDiscountMetricCollector::DiscountConsentStatus::DECLINED, 1);
}

TEST_F(CartServiceDiscountTest, TestRecordDiscountConsentStatus_Accepted) {
  base::RunLoop run_loop[2];
  // Simulate that the welcome surface is not showing, the discount feature is
  // disabled and there is no partner merchant carts.
  profile_->GetPrefs()->SetInteger(prefs::kCartModuleWelcomeSurfaceShownTimes,
                                   CartService::kWelcomSurfaceShowLimit);
  service_->GetDB()->DeleteAllCarts(
      base::BindOnce(&CartServiceTest::OperationEvaluation,
                     base::Unretained(this), run_loop[0].QuitClosure(), true));
  run_loop[0].Run();

  // Add a non-partner-merchant cart, and simulate that user has accepted the
  // consent.
  service_->AddCart(mock_merchant_url_A_, std::nullopt, kMockProtoA);
  task_environment_.RunUntilIdle();
  profile_->GetPrefs()->SetBoolean(prefs::kCartDiscountAcknowledged, true);
  profile_->GetPrefs()->SetBoolean(prefs::kCartDiscountEnabled, true);

  service_->ShouldShowDiscountConsent(
      base::BindOnce(&CartServiceTest::GetEvaluationBoolResult,
                     base::Unretained(this), run_loop[1].QuitClosure(), false));
  run_loop[1].Run();
  histogram_tester_.ExpectBucketCount(
      "NewTabPage.Carts.DiscountConsentStatusAtLoad",
      CartDiscountMetricCollector::DiscountConsentStatus::ACCEPTED, 1);
}

class CartServiceMerchantWideDiscountTest : public CartServiceTest {
 public:
  // Features need to be initialized before CartServiceTest::SetUp runs, in
  // order to avoid tsan data race error on FeatureList.
  CartServiceMerchantWideDiscountTest() {
    std::vector<base::test::FeatureRefAndParams> enabled_features;
    base::FieldTrialParams cart_params;
    cart_params[ntp_features::kNtpChromeCartModuleAbandonedCartDiscountParam] =
        "true";
    enabled_features.emplace_back(ntp_features::kNtpChromeCartModule,
                                  cart_params);

    features_.InitWithFeaturesAndParameters(enabled_features, {});
  }

  void SetUp() override {
    CartServiceTest::SetUp();

    auto& data = commerce_heuristics::CommerceHeuristicsData::GetInstance();
    ASSERT_TRUE(data.PopulateDataFromComponent("{}", kGlobalHeuristicsJSONData,
                                               "", ""));
  }
};

TEST_F(CartServiceMerchantWideDiscountTest, TestDiscountConsentShown) {
  // Add a merchant cart.
  service_->AddCart(mock_merchant_url_A_, std::nullopt, kMockProtoA);
  task_environment_.RunUntilIdle();

  base::RunLoop run_loop;
  for (int i = 0; i < CartService::kWelcomSurfaceShowLimit + 1; i++) {
    service_->IncreaseWelcomeSurfaceCounter();
  }
  ASSERT_FALSE(service_->ShouldShowWelcomeSurface());
  ASSERT_FALSE(
      profile_->GetPrefs()->GetBoolean(prefs::kCartDiscountConsentShown));

  service_->ShouldShowDiscountConsent(
      base::BindOnce(&CartServiceTest::GetEvaluationBoolResult,
                     base::Unretained(this), run_loop.QuitClosure(), true));
  run_loop.Run();
  ASSERT_TRUE(
      profile_->GetPrefs()->GetBoolean(prefs::kCartDiscountConsentShown));
}

TEST_F(CartServiceMerchantWideDiscountTest,
       TestNoDiscountConsentShownForNoDiscountMerchant) {
  // Add a no-discount merchant cart.
  service_->AddCart(GURL(kNoDiscountMerchantURL), std::nullopt,
                    BuildProto(kNoDiscountMerchant, kNoDiscountMerchantURL));
  task_environment_.RunUntilIdle();

  base::RunLoop run_loop;
  for (int i = 0; i < CartService::kWelcomSurfaceShowLimit + 1; i++) {
    service_->IncreaseWelcomeSurfaceCounter();
  }
  ASSERT_FALSE(service_->ShouldShowWelcomeSurface());
  ASSERT_FALSE(
      profile_->GetPrefs()->GetBoolean(prefs::kCartDiscountConsentShown));

  service_->ShouldShowDiscountConsent(
      base::BindOnce(&CartServiceTest::GetEvaluationBoolResult,
                     base::Unretained(this), run_loop.QuitClosure(), false));
  run_loop.Run();
  ASSERT_FALSE(
      profile_->GetPrefs()->GetBoolean(prefs::kCartDiscountConsentShown));
}

class CartServiceSkipExtractionTest : public CartServiceTest {
 public:
  // Features need to be initialized before CartServiceTest::SetUp runs, in
  // order to avoid tsan data race error on FeatureList.
  CartServiceSkipExtractionTest() {
    features_.InitAndEnableFeatureWithParameters(
        ntp_features::kNtpChromeCartModule,
        {{"skip-cart-extraction-pattern", kMockMerchantC}});
  }
};

TEST_F(CartServiceSkipExtractionTest, TestAddCartForSkippedMerchants) {
  base::RunLoop run_loop[4];
  CartDB* cart_db_ = service_->GetDB();
  // Product images are not stored for skipped merchants.
  service_->AddCart(mock_merchant_url_C_, std::nullopt, kMockProtoCWithProduct);
  task_environment_.RunUntilIdle();
  cart_db_->LoadAllCarts(base::BindOnce(&CartServiceTest::GetEvaluationURL,
                                        base::Unretained(this),
                                        run_loop[0].QuitClosure(), kExpectedC));
  run_loop[0].Run();

  // Product images are overwritten for skipped merchants.
  cart_db_->AddCart(
      kMockMerchantC, kMockProtoCWithProduct,
      base::BindOnce(&CartServiceTest::OperationEvaluation,
                     base::Unretained(this), run_loop[1].QuitClosure(), true));
  run_loop[1].Run();
  cart_db_->LoadAllCarts(
      base::BindOnce(&CartServiceTest::GetEvaluationURL, base::Unretained(this),
                     run_loop[2].QuitClosure(), kExpectedCWithProduct));
  run_loop[2].Run();
  service_->AddCart(mock_merchant_url_C_, std::nullopt, kMockProtoCWithProduct);
  task_environment_.RunUntilIdle();
  cart_db_->LoadAllCarts(base::BindOnce(&CartServiceTest::GetEvaluationURL,
                                        base::Unretained(this),
                                        run_loop[3].QuitClosure(), kExpectedC));
  run_loop[3].Run();
}

TEST_F(CartServiceSkipExtractionTest, TestLoadCartForSkippedMerchants) {
  base::RunLoop run_loop[4];
  CartDB* cart_db_ = service_->GetDB();
  cart_db_->AddCart(
      kMockMerchantC, kMockProtoCWithProduct,
      base::BindOnce(&CartServiceTest::OperationEvaluation,
                     base::Unretained(this), run_loop[0].QuitClosure(), true));
  run_loop[0].Run();
  cart_db_->LoadAllCarts(
      base::BindOnce(&CartServiceTest::GetEvaluationURL, base::Unretained(this),
                     run_loop[1].QuitClosure(), kExpectedCWithProduct));
  run_loop[1].Run();
  // Skipped carts will not show product images when loading, and the existing
  // product images in skipped carts will also be cleared.
  service_->LoadAllActiveCarts(
      base::BindOnce(&CartServiceTest::GetEvaluationURL, base::Unretained(this),
                     run_loop[2].QuitClosure(), kExpectedC));
  run_loop[2].Run();
  cart_db_->LoadAllCarts(base::BindOnce(&CartServiceTest::GetEvaluationURL,
                                        base::Unretained(this),
                                        run_loop[3].QuitClosure(), kExpectedC));
  run_loop[3].Run();
}

class CartServiceCartURLUTMTest : public CartServiceTest {
 public:
  CartServiceCartURLUTMTest() {
    // This needs to be called before any tasks that run on other threads check
    // if a feature is enabled.
    features_.InitAndEnableFeatureWithParameters(
        ntp_features::kNtpChromeCartModule,
        {{ntp_features::kNtpChromeCartModuleAbandonedCartDiscountParam,
          "true"}});
  }
  void SetUp() override {
    CartServiceTest::SetUp();

    auto& data = commerce_heuristics::CommerceHeuristicsData::GetInstance();
    ASSERT_TRUE(data.PopulateDataFromComponent("{}", R"###(
        {
          "rule_discount_partner_merchant_regex": "(foo.com)"
        }
    )###",
                                               "", ""));
  }
  void TearDown() override {
    // Set the feature to default disabled state after test.
    profile_->GetPrefs()->SetBoolean(prefs::kCartDiscountEnabled, false);
  }
};

TEST_F(CartServiceCartURLUTMTest, TestAppendUTMForPartnerMerchants) {
  EXPECT_FALSE(service_->IsCartDiscountEnabled());
  EXPECT_EQ(GURL(kMockMerchantURLWithNoDiscountUtmA),
            service_->AppendUTM(GURL(kMockMerchantURLA)));

  profile_->GetPrefs()->SetBoolean(prefs::kCartDiscountEnabled, true);
  EXPECT_TRUE(service_->IsCartDiscountEnabled());
  EXPECT_EQ(GURL(kMockMerchantURLWithDiscountUtmA),
            service_->AppendUTM(GURL(kMockMerchantURLA)));
}

TEST_F(CartServiceCartURLUTMTest, TestAppendUTMForNonPartnerMerchants) {
  EXPECT_FALSE(service_->IsCartDiscountEnabled());
  EXPECT_EQ(GURL(kMockMerchantURLWithCartUtmC),
            service_->AppendUTM(GURL(kMockMerchantURLC)));

  profile_->GetPrefs()->SetBoolean(prefs::kCartDiscountEnabled, true);
  EXPECT_TRUE(service_->IsCartDiscountEnabled());
  EXPECT_EQ(GURL(kMockMerchantURLWithCartUtmC),
            service_->AppendUTM(GURL(kMockMerchantURLC)));
}

TEST_F(CartServiceCartURLUTMTest, TestAppendUTMAvoidDuplicates) {
  GURL merchantUrl = GURL(kMockMerchantURLA);
  EXPECT_FALSE(service_->IsCartDiscountEnabled());
  merchantUrl = service_->AppendUTM(GURL(merchantUrl));
  EXPECT_EQ(GURL(kMockMerchantURLWithNoDiscountUtmA), merchantUrl);

  profile_->GetPrefs()->SetBoolean(prefs::kCartDiscountEnabled, true);
  EXPECT_TRUE(service_->IsCartDiscountEnabled());
  merchantUrl = service_->AppendUTM(GURL(merchantUrl));
  EXPECT_EQ(GURL(kMockMerchantURLWithDiscountUtmA), merchantUrl);
}

class FakeFetchDiscountWorker : public FetchDiscountWorker {
 public:
  FakeFetchDiscountWorker(
      scoped_refptr<network::SharedURLLoaderFactory>
          browserProcessURLLoaderFactory,
      std::unique_ptr<CartDiscountFetcherFactory> fetcher_factory,
      std::unique_ptr<CartDiscountServiceDelegate>
          cart_discount_service_delegate,
      signin::IdentityManager* const identity_manager,
      variations::VariationsClient* const chrome_variations_client)
      : FetchDiscountWorker(browserProcessURLLoaderFactory,
                            std::move(fetcher_factory),
                            std::move(cart_discount_service_delegate),
                            identity_manager,
                            chrome_variations_client) {}

  // Simulate FetchDiscountWorker posting a task to fetch, except that here we
  // only record fetch timestamp instead of actually fetching.
  void Start(base::TimeDelta delay) override {
    content::GetUIThreadTaskRunner({base::TaskPriority::BEST_EFFORT})
        ->PostDelayedTask(FROM_HERE,
                          base::BindOnce(&FakeFetchDiscountWorker::FakeFetch,
                                         weak_ptr_factory_.GetWeakPtr()),
                          delay);
  }

 private:
  void FakeFetch() { cart_discount_service_delegate_->RecordFetchTimestamp(); }

  base::WeakPtrFactory<FakeFetchDiscountWorker> weak_ptr_factory_{this};
};

class CartServiceDiscountFetchTest : public CartServiceTest {
 public:
  CartServiceDiscountFetchTest() {
    // This needs to be called before any tasks that run on other threads check
    // if a feature is enabled.
    features_.InitAndEnableFeatureWithParameters(
        ntp_features::kNtpChromeCartModule,
        {{ntp_features::kNtpChromeCartModuleAbandonedCartDiscountParam, "true"},
         {"discount-fetch-delay", "2s"}});
  }

  void SetUp() override {
    CartServiceTest::SetUp();
    profile_->GetPrefs()->SetBoolean(prefs::kCartDiscountEnabled, true);
    // Only initialize CartServiceDelegate which is relevant to this test.
    fetch_discount_worker_ = std::make_unique<FakeFetchDiscountWorker>(
        nullptr, nullptr,
        std::make_unique<CartDiscountServiceDelegate>(service_), nullptr,
        nullptr);
    service_->SetFetchDiscountWorkerForTesting(
        std::move(fetch_discount_worker_));
  }

  void TearDown() override {
    // Reset the last fetch timestamp.
    profile_->GetPrefs()->SetTime(prefs::kCartDiscountLastFetchedTime,
                                  base::Time());
    // Reset FetchDiscountWorker for testing.
    service_->SetFetchDiscountWorkerForTesting(nullptr);
  }

  void StartGettingDiscount() { service_->StartGettingDiscount(); }

 private:
  std::unique_ptr<FakeFetchDiscountWorker> fetch_discount_worker_;
};

TEST_F(CartServiceDiscountFetchTest, TestFreshFetch) {
  EXPECT_EQ(profile_->GetPrefs()->GetTime(prefs::kCartDiscountLastFetchedTime),
            base::Time());
  StartGettingDiscount();
  task_environment_.RunUntilIdle();
  EXPECT_NE(profile_->GetPrefs()->GetTime(prefs::kCartDiscountLastFetchedTime),
            base::Time());
}

TEST_F(CartServiceDiscountFetchTest, TestFetchWhenBeyondEnforcedDelay) {
  // Set last fetch timestamp so that the current time is beyond the enforced
  // delay.
  base::Time last_fetch_time = base::Time::Now() - base::Seconds(20);
  profile_->GetPrefs()->SetTime(prefs::kCartDiscountLastFetchedTime,
                                last_fetch_time);
  StartGettingDiscount();
  task_environment_.RunUntilIdle();
  EXPECT_NE(profile_->GetPrefs()->GetTime(prefs::kCartDiscountLastFetchedTime),
            last_fetch_time);
}

TEST_F(CartServiceDiscountFetchTest, TestNoFetchWithinEnforcedDelay) {
  EXPECT_EQ(profile_->GetPrefs()->GetTime(prefs::kCartDiscountLastFetchedTime),
            base::Time());
  base::Time last_fetch_time = base::Time::Now();
  profile_->GetPrefs()->SetTime(prefs::kCartDiscountLastFetchedTime,
                                last_fetch_time);
  // Set last fetch timestamp so that the current time is within the enforced
  // delay.
  StartGettingDiscount();
  task_environment_.RunUntilIdle();
  EXPECT_EQ(profile_->GetPrefs()->GetTime(prefs::kCartDiscountLastFetchedTime),
            last_fetch_time);
  // Wait so that the current time is beyond the enforced delay.
  task_environment_.FastForwardBy(base::Seconds(2));
  StartGettingDiscount();
  task_environment_.RunUntilIdle();
  EXPECT_NE(profile_->GetPrefs()->GetTime(prefs::kCartDiscountLastFetchedTime),
            last_fetch_time);
}

class CartServiceCouponTest : public CartServiceTest {
 public:
  void SetUp() override {
    CartServiceTest::SetUp();
    SetCouponServiceForTesting(&coupon_service_);
  }

 protected:
  class MockCouponService : public CouponService {
   public:
    MOCK_METHOD0(DeleteAllFreeListingCoupons, void(void));
    MOCK_METHOD1(DeleteFreeListingCouponsForUrl, void(const GURL& url));
    MOCK_METHOD1(MaybeFeatureStatusChanged, void(bool enabled));
  };

  void SetCouponServiceForTesting(CouponService* coupon_service) {
    service_->SetCouponServiceForTesting(coupon_service);
  }

  MockCouponService coupon_service_;
};

TEST_F(CartServiceCouponTest, TestDeleteCartWithCoupon_DeleteImmediately) {
  const GURL& url = GURL(kMockMerchantURLA);
  EXPECT_CALL(coupon_service_, DeleteFreeListingCouponsForUrl(url)).Times(1);
  // Coupons are deleted immediately when the cart is deleted in a way that
  // ignores removal status. In this case, the cart is also deleted immediately
  // and doesn't go through deletion pending.
  service_->DeleteCart(url, true);
}

TEST_F(CartServiceCouponTest,
       TestDeleteCartWithCoupon_NotDeleteCouponImmediately) {
  service_->AddCart(mock_merchant_url_A_, std::nullopt, kMockProtoA);
  task_environment_.RunUntilIdle();

  const GURL& url = GURL(kMockMerchantURLA);
  EXPECT_CALL(coupon_service_, DeleteFreeListingCouponsForUrl(url)).Times(0);
  // Coupons are not deleted until deletion time is reached.
  service_->DeleteCart(url, false);
  task_environment_.FastForwardBy(
      commerce::kCodeBasedRuleDiscountCouponDeletionTime.Get() -
      base::Seconds(2));
  task_environment_.RunUntilIdle();
}

TEST_F(CartServiceCouponTest,
       TestDeleteCartWithCoupon_DeleteCouponForActualDeletion) {
  service_->AddCart(mock_merchant_url_A_, std::nullopt, kMockProtoA);
  task_environment_.RunUntilIdle();

  const GURL& url = GURL(kMockMerchantURLA);
  EXPECT_CALL(coupon_service_, DeleteFreeListingCouponsForUrl(url)).Times(1);
  // Coupons are deleted when the cart is actually deleted.
  service_->DeleteCart(url, false);
  task_environment_.FastForwardBy(
      commerce::kCodeBasedRuleDiscountCouponDeletionTime.Get());
  task_environment_.RunUntilIdle();
}

TEST_F(CartServiceCouponTest,
       TestDeleteCartWithCoupon_NotDeleteCouponForCanceledDeletion) {
  service_->AddCart(mock_merchant_url_A_, std::nullopt, kMockProtoA);
  task_environment_.RunUntilIdle();

  const GURL& url = GURL(kMockMerchantURLA);
  EXPECT_CALL(coupon_service_, DeleteFreeListingCouponsForUrl(url)).Times(0);
  // Coupons are never deleted when the cart is not actually deleted.
  service_->DeleteCart(url, false);
  service_->AddCart(url, std::nullopt, kMockProtoA);
  task_environment_.FastForwardBy(
      commerce::kCodeBasedRuleDiscountCouponDeletionTime.Get());
  task_environment_.RunUntilIdle();
}

TEST_F(CartServiceCouponTest, TestClearCoupons) {
  EXPECT_CALL(coupon_service_, DeleteAllFreeListingCoupons()).Times(1);

  service_->OnHistoryDeletions(
      HistoryServiceFactory::GetForProfile(profile_.get(),
                                           ServiceAccessType::EXPLICIT_ACCESS),
      history::DeletionInfo(history::DeletionTimeRange::Invalid(), false,
                            history::URLRows(), std::set<GURL>(),
                            std::nullopt));
}
TEST_F(CartServiceCouponTest, TestUpdateCartDeleteCoupon_AddProduct) {
  const GURL& url = GURL(kMockMerchantURLA);
  EXPECT_CALL(coupon_service_, DeleteFreeListingCouponsForUrl(url)).Times(0);
  // Construct a proto with one product.
  cart_db::ChromeCartContentProto proto =
      BuildProto(kMockMerchantA, kMockMerchantURLA);
  proto.add_product_image_urls("https://image1.com");
  auto* added_product = proto.add_product_infos();
  *added_product = kMockProductA;
  service_->AddCart(mock_merchant_url_A_, std::nullopt, proto);
  task_environment_.RunUntilIdle();

  // A new cart added with new products will not delete coupons.
  added_product = proto.add_product_infos();
  *added_product = kMockProductB;
  service_->AddCart(mock_merchant_url_A_, std::nullopt, proto);
  task_environment_.RunUntilIdle();
}

TEST_F(CartServiceCouponTest, TestUpdateCartDeleteCoupon_DeleteProduct) {
  const GURL& url = GURL(kMockMerchantURLA);
  EXPECT_CALL(coupon_service_, DeleteFreeListingCouponsForUrl(url)).Times(1);
  // Construct a proto with two products.
  cart_db::ChromeCartContentProto proto =
      BuildProto(kMockMerchantA, kMockMerchantURLA);
  proto.add_product_image_urls("https://image1.com");
  auto* added_product = proto.add_product_infos();
  *added_product = kMockProductA;
  added_product = proto.add_product_infos();
  *added_product = kMockProductB;
  service_->AddCart(mock_merchant_url_A_, std::nullopt, proto);
  task_environment_.RunUntilIdle();

  // A new cart added with one product removed will trigger coupon deletion.
  proto.clear_product_infos();
  added_product = proto.add_product_infos();
  *added_product = kMockProductA;
  service_->AddCart(mock_merchant_url_A_, std::nullopt, proto);
  task_environment_.RunUntilIdle();
}

TEST_F(CartServiceCouponTest, TestUpdateCartDeleteCoupon_ReplaceProduct) {
  const GURL& url = GURL(kMockMerchantURLA);
  EXPECT_CALL(coupon_service_, DeleteFreeListingCouponsForUrl(url)).Times(1);
  // Construct a proto with two products.
  cart_db::ChromeCartContentProto proto =
      BuildProto(kMockMerchantA, kMockMerchantURLA);
  proto.add_product_image_urls("https://image1.com");
  auto* added_product = proto.add_product_infos();
  *added_product = kMockProductA;
  added_product = proto.add_product_infos();
  *added_product = kMockProductB;
  service_->AddCart(mock_merchant_url_A_, std::nullopt, proto);
  task_environment_.RunUntilIdle();

  // A new cart added with one product replaced will trigger coupon deletion.
  proto.clear_product_infos();
  added_product = proto.add_product_infos();
  *added_product = kMockProductA;
  added_product = proto.add_product_infos();
  *added_product = BuildProductProto("id_qux");
  service_->AddCart(mock_merchant_url_A_, std::nullopt, proto);
  task_environment_.RunUntilIdle();
}

TEST_F(CartServiceCouponTest, TestRBDFeatureStatusUpdate) {
  profile_->GetPrefs()->SetBoolean(prefs::kCartDiscountEnabled, true);

  EXPECT_CALL(coupon_service_, MaybeFeatureStatusChanged(false)).Times(1);
  profile_->GetPrefs()->SetBoolean(prefs::kCartDiscountEnabled, false);

  EXPECT_CALL(coupon_service_, MaybeFeatureStatusChanged(true)).Times(1);
  profile_->GetPrefs()->SetBoolean(prefs::kCartDiscountEnabled, true);
}

TEST_F(CartServiceCouponTest, TestCartFeatureStatusUpdate) {
  profile_->GetPrefs()->SetBoolean(prefs::kCartDiscountEnabled, true);

  EXPECT_CALL(coupon_service_, MaybeFeatureStatusChanged(false)).Times(2);
  ScopedListPrefUpdate(profile_->GetPrefs(), prefs::kNtpDisabledModules)
      ->Append("chrome_cart");
  ScopedListPrefUpdate(profile_->GetPrefs(), prefs::kNtpDisabledModules)
      ->Append("something_unrelated");

  EXPECT_CALL(coupon_service_, MaybeFeatureStatusChanged(true)).Times(1);
  ScopedListPrefUpdate(profile_->GetPrefs(), prefs::kNtpDisabledModules)
      ->EraseValue(base::Value("chrome_cart"));
}

TEST_F(CartServiceCouponTest, TestModuleFeatureStatusUpdate) {
  // prefs::kNtpModulesVisible is true by default.
  profile_->GetPrefs()->SetBoolean(prefs::kCartDiscountEnabled, true);

  EXPECT_CALL(coupon_service_, MaybeFeatureStatusChanged(false)).Times(1);
  profile_->GetPrefs()->SetBoolean(prefs::kNtpModulesVisible, false);

  EXPECT_CALL(coupon_service_, MaybeFeatureStatusChanged(true)).Times(1);
  profile_->GetPrefs()->SetBoolean(prefs::kNtpModulesVisible, true);
}

class CartServiceDiscountConsentV2Test : public CartServiceTest {
 public:
  // Features need to be initialized before CartServiceTest::SetUp runs, in
  // order to avoid tsan data race error on FeatureList.
  CartServiceDiscountConsentV2Test() {
    std::vector<base::test::FeatureRefAndParams> enabled_features;
    base::FieldTrialParams consent_v2_params, cart_params;
    cart_params[ntp_features::kNtpChromeCartModuleAbandonedCartDiscountParam] =
        "true";
    enabled_features.emplace_back(ntp_features::kNtpChromeCartModule,
                                  cart_params);
    consent_v2_params["discount-consent-ntp-reshow-time"] = "1m";
    consent_v2_params["discount-consent-ntp-max-dismiss-count"] = "2";
    consent_v2_params["discount-consent-ntp-variation"] = "2";
    enabled_features.emplace_back(commerce::kDiscountConsentV2,
                                  consent_v2_params);
    features_.InitWithFeaturesAndParameters(enabled_features,
                                            /*disabled_features*/ {});

    auto& data = commerce_heuristics::CommerceHeuristicsData::GetInstance();
    data.PopulateDataFromComponent("{}", R"###(
        {
          "rule_discount_partner_merchant_regex": "(foo.com)"
        }
    )###",
                                   "", "");
  }

  void SetUp() override {
    CartServiceTest::SetUp();

    // Add a partner merchant cart.
    service_->AddCart(mock_merchant_url_A_, std::nullopt, kMockProtoA);
    task_environment_.RunUntilIdle();
    // Simulate that the welcome surface is not showing, the discount feature is
    // disabled and there is no partner merchant carts.
    profile_->GetPrefs()->SetInteger(prefs::kCartModuleWelcomeSurfaceShownTimes,
                                     CartService::kWelcomSurfaceShowLimit);
  }

  void TearDown() override {
    profile_->GetPrefs()->SetInteger(prefs::kDiscountConsentPastDismissedCount,
                                     0);
    profile_->GetPrefs()->SetInteger(prefs::kCartModuleWelcomeSurfaceShownTimes,
                                     0);
  }

  void RecordDiscountConsentStatusAtLoad(bool should_show_consent) {
    service_->RecordDiscountConsentStatusAtLoad(should_show_consent);
  }
};

// Tests discount consent doesn't show after dismiss count reach the max
// allowance.
TEST_F(CartServiceDiscountConsentV2Test, TestNoConsentAfterDimissAllowance) {
  // Simulate that use has dismissed the consent once.
  profile_->GetPrefs()->SetInteger(prefs::kDiscountConsentPastDismissedCount,
                                   1);
  {
    base::RunLoop run_loop;

    service_->ShouldShowDiscountConsent(
        base::BindOnce(&CartServiceTest::GetEvaluationBoolResult,
                       base::Unretained(this), run_loop.QuitClosure(), true));
    run_loop.Run();
  }

  service_->DismissedDiscountConsent();
  {
    base::RunLoop run_loop;
    service_->ShouldShowDiscountConsent(
        base::BindOnce(&CartServiceTest::GetEvaluationBoolResult,
                       base::Unretained(this), run_loop.QuitClosure(), false));
    run_loop.Run();
  }
}

// Tests discount consent doesn't show if reshow time threshold does not meet.
TEST_F(CartServiceDiscountConsentV2Test,
       TestNoConsentBeforeReshowTimeThreshold) {
  {
    base::RunLoop run_loop;
    service_->ShouldShowDiscountConsent(
        base::BindOnce(&CartServiceTest::GetEvaluationBoolResult,
                       base::Unretained(this), run_loop.QuitClosure(), true));
    run_loop.Run();
  }

  service_->DismissedDiscountConsent();
  {
    base::RunLoop run_loop;
    service_->ShouldShowDiscountConsent(
        base::BindOnce(&CartServiceTest::GetEvaluationBoolResult,
                       base::Unretained(this), run_loop.QuitClosure(), false));
    run_loop.Run();
  }
}

// Tests discount consent reshow after the reshow time threshold.
TEST_F(CartServiceDiscountConsentV2Test,
       TestReshowConsentAfterReshowTimeThreshold) {
  {
    base::RunLoop run_loop;
    service_->ShouldShowDiscountConsent(
        base::BindOnce(&CartServiceTest::GetEvaluationBoolResult,
                       base::Unretained(this), run_loop.QuitClosure(), true));
    run_loop.Run();
  }

  service_->DismissedDiscountConsent();

  {
    base::RunLoop run_loop;
    service_->ShouldShowDiscountConsent(
        base::BindOnce(&CartServiceTest::GetEvaluationBoolResult,
                       base::Unretained(this), run_loop.QuitClosure(), false));
    run_loop.Run();
  }

  task_environment_.FastForwardBy(base::Minutes(2));

  {
    base::RunLoop run_loop;
    service_->ShouldShowDiscountConsent(
        base::BindOnce(&CartServiceTest::GetEvaluationBoolResult,
                       base::Unretained(this), run_loop.QuitClosure(), true));
    run_loop.Run();
  }
}

TEST_F(CartServiceDiscountConsentV2Test,
       TestRecordDiscountConsentStatus_AcceptedInVariation) {
  // Simulate consent has been accepted in the default variation.
  profile_->GetPrefs()->SetInteger(prefs::kDiscountConsentDecisionMadeIn, 0);
  profile_->GetPrefs()->SetBoolean(prefs::kCartDiscountAcknowledged, true);
  profile_->GetPrefs()->SetBoolean(prefs::kCartDiscountEnabled, true);
  bool should_show = false;

  histogram_tester_.ExpectBucketCount(
      "NewTabPage.Carts.DiscountConsentStatusAtLoad.AcceptedIn",
      commerce::DiscountConsentNtpVariation::kDefault, 0);

  RecordDiscountConsentStatusAtLoad(should_show);
  histogram_tester_.ExpectBucketCount(
      "NewTabPage.Carts.DiscountConsentStatusAtLoad.AcceptedIn",
      commerce::DiscountConsentNtpVariation::kDefault, 1);

  // Simulate consent has been accepted in the Inline variation.
  profile_->GetPrefs()->SetInteger(prefs::kDiscountConsentDecisionMadeIn, 2);
  histogram_tester_.ExpectBucketCount(
      "NewTabPage.Carts.DiscountConsentStatusAtLoad.AcceptedIn",
      commerce::DiscountConsentNtpVariation::kInline, 0);

  RecordDiscountConsentStatusAtLoad(should_show);
  histogram_tester_.ExpectBucketCount(
      "NewTabPage.Carts.DiscountConsentStatusAtLoad.AcceptedIn",
      commerce::DiscountConsentNtpVariation::kInline, 1);
}

TEST_F(CartServiceDiscountConsentV2Test,
       TestRecordDiscountConsentStatus_RejectedInVariation) {
  // Simulate consent has been rejected in the Default variation.
  profile_->GetPrefs()->SetInteger(prefs::kDiscountConsentDecisionMadeIn, 0);
  profile_->GetPrefs()->SetBoolean(prefs::kCartDiscountAcknowledged, true);
  profile_->GetPrefs()->SetBoolean(prefs::kCartDiscountEnabled, false);
  bool should_show = false;

  histogram_tester_.ExpectBucketCount(
      "NewTabPage.Carts.DiscountConsentStatusAtLoad.RejectedIn",
      commerce::DiscountConsentNtpVariation::kDefault, 0);

  RecordDiscountConsentStatusAtLoad(should_show);
  histogram_tester_.ExpectBucketCount(
      "NewTabPage.Carts.DiscountConsentStatusAtLoad.RejectedIn",
      commerce::DiscountConsentNtpVariation::kDefault, 1);

  // Simulate consent has been rejected in the Inline variation.
  profile_->GetPrefs()->SetInteger(prefs::kDiscountConsentDecisionMadeIn, 2);
  histogram_tester_.ExpectBucketCount(
      "NewTabPage.Carts.DiscountConsentStatusAtLoad.RejectedIn",
      commerce::DiscountConsentNtpVariation::kInline, 0);

  RecordDiscountConsentStatusAtLoad(should_show);
  histogram_tester_.ExpectBucketCount(
      "NewTabPage.Carts.DiscountConsentStatusAtLoad.RejectedIn",
      commerce::DiscountConsentNtpVariation::kInline, 1);
}

TEST_F(CartServiceDiscountConsentV2Test,
       TestRecordDiscountConsentStatus_NoShowAfterDecidedInVariation) {
  // Simulate consent has been rejected in the Default variation.
  profile_->GetPrefs()->SetBoolean(prefs::kCartDiscountAcknowledged, true);
  profile_->GetPrefs()->SetBoolean(prefs::kCartDiscountEnabled, false);
  profile_->GetPrefs()->SetInteger(prefs::kDiscountConsentDecisionMadeIn, 0);
  bool should_show = false;

  histogram_tester_.ExpectBucketCount(
      "NewTabPage.Carts.DiscountConsentStatusAtLoad.NoShowHasFinalized",
      commerce::DiscountConsentNtpVariation::kDefault, 0);

  RecordDiscountConsentStatusAtLoad(should_show);
  histogram_tester_.ExpectBucketCount(
      "NewTabPage.Carts.DiscountConsentStatusAtLoad.NoShowHasFinalized",
      commerce::DiscountConsentNtpVariation::kDefault, 1);

  // Simulate consent has been accepted in the Default variation.
  profile_->GetPrefs()->SetBoolean(prefs::kCartDiscountEnabled, true);

  RecordDiscountConsentStatusAtLoad(should_show);
  histogram_tester_.ExpectBucketCount(
      "NewTabPage.Carts.DiscountConsentStatusAtLoad.NoShowHasFinalized",
      commerce::DiscountConsentNtpVariation::kDefault, 2);

  // Simulate consent has been accepted in the Inline variation.
  profile_->GetPrefs()->SetBoolean(prefs::kCartDiscountEnabled, true);
  profile_->GetPrefs()->SetInteger(prefs::kDiscountConsentDecisionMadeIn, 2);

  histogram_tester_.ExpectBucketCount(
      "NewTabPage.Carts.DiscountConsentStatusAtLoad.NoShowHasFinalized",
      commerce::DiscountConsentNtpVariation::kInline, 0);

  RecordDiscountConsentStatusAtLoad(should_show);
  histogram_tester_.ExpectBucketCount(
      "NewTabPage.Carts.DiscountConsentStatusAtLoad.NoShowHasFinalized",
      commerce::DiscountConsentNtpVariation::kInline, 1);
}

TEST_F(CartServiceDiscountConsentV2Test,
       TestRecordDiscountConsentStatus_DismissedInVariation) {
  // Simulate consent has been dismissed in the Inline variation.
  profile_->GetPrefs()->SetBoolean(prefs::kCartDiscountAcknowledged, false);
  profile_->GetPrefs()->SetInteger(prefs::kDiscountConsentDismissedIn, 2);
  profile_->GetPrefs()->SetInteger(prefs::kDiscountConsentPastDismissedCount,
                                   1);
  bool should_show = true;

  histogram_tester_.ExpectBucketCount(
      "NewTabPage.Carts.DiscountConsentStatusAtLoad.DismissedIn",
      commerce::DiscountConsentNtpVariation::kInline, 0);

  RecordDiscountConsentStatusAtLoad(should_show);
  histogram_tester_.ExpectBucketCount(
      "NewTabPage.Carts.DiscountConsentStatusAtLoad.DismissedIn",
      commerce::DiscountConsentNtpVariation::kInline, 1);

  // Simulate consent has been dismissed in the Dialog variation.
  profile_->GetPrefs()->SetInteger(prefs::kDiscountConsentDismissedIn, 3);

  histogram_tester_.ExpectBucketCount(
      "NewTabPage.Carts.DiscountConsentStatusAtLoad.DismissedIn",
      commerce::DiscountConsentNtpVariation::kDialog, 0);

  RecordDiscountConsentStatusAtLoad(should_show);
  histogram_tester_.ExpectBucketCount(
      "NewTabPage.Carts.DiscountConsentStatusAtLoad.DismissedIn",
      commerce::DiscountConsentNtpVariation::kDialog, 1);
}

TEST_F(CartServiceDiscountConsentV2Test,
       TestRecordDiscountConsentStatus_ShowInterestInVariation) {
  // Simulate 'continue' button is clicked in the Inline variation.
  profile_->GetPrefs()->SetBoolean(prefs::kCartDiscountAcknowledged, false);
  profile_->GetPrefs()->SetBoolean(prefs::kDiscountConsentShowInterest, true);
  profile_->GetPrefs()->SetInteger(prefs::kDiscountConsentShowInterestIn, 2);
  bool should_show = true;

  histogram_tester_.ExpectBucketCount(
      "NewTabPage.Carts.DiscountConsentStatusAtLoad.InterestedButNoActionIn",
      commerce::DiscountConsentNtpVariation::kInline, 0);

  RecordDiscountConsentStatusAtLoad(should_show);
  histogram_tester_.ExpectBucketCount(
      "NewTabPage.Carts.DiscountConsentStatusAtLoad.InterestedButNoActionIn",
      commerce::DiscountConsentNtpVariation::kInline, 1);

  // Simulate 'continue' button is clicked in the Dialog variation.
  profile_->GetPrefs()->SetInteger(prefs::kDiscountConsentShowInterestIn, 3);
  histogram_tester_.ExpectBucketCount(
      "NewTabPage.Carts.DiscountConsentStatusAtLoad.InterestedButNoActionIn",
      commerce::DiscountConsentNtpVariation::kDialog, 0);

  RecordDiscountConsentStatusAtLoad(should_show);
  histogram_tester_.ExpectBucketCount(
      "NewTabPage.Carts.DiscountConsentStatusAtLoad.InterestedButNoActionIn",
      commerce::DiscountConsentNtpVariation::kDialog, 1);
}

TEST_F(CartServiceDiscountConsentV2Test,
       TestRecordDiscountConsentStatus_NeverShowInVariation) {
  // Simulate consent is shown in the Default variation before. And the current
  // variation is Inline.
  profile_->GetPrefs()->SetBoolean(prefs::kCartDiscountAcknowledged, false);
  profile_->GetPrefs()->SetInteger(prefs::kDiscountConsentLastShownInVariation,
                                   0);
  bool should_show = false;

  histogram_tester_.ExpectBucketCount(
      "NewTabPage.Carts.DiscountConsentStatusAtLoad.NeverShownIn",
      commerce::DiscountConsentNtpVariation::kInline, 0);

  RecordDiscountConsentStatusAtLoad(should_show);
  histogram_tester_.ExpectBucketCount(
      "NewTabPage.Carts.DiscountConsentStatusAtLoad.NeverShownIn",
      commerce::DiscountConsentNtpVariation::kInline, 1);

  // Simulate consent is shown in the Inline variation before.
  profile_->GetPrefs()->SetInteger(prefs::kDiscountConsentLastShownInVariation,
                                   2);

  RecordDiscountConsentStatusAtLoad(should_show);
  histogram_tester_.ExpectBucketCount(
      "NewTabPage.Carts.DiscountConsentStatusAtLoad.NeverShownIn",
      commerce::DiscountConsentNtpVariation::kInline, 1);
}

TEST_F(CartServiceDiscountConsentV2Test,
       TestRecordDiscountConsentStatus_NoShowInVariation) {
  // Simulate the consent shown in the Inline variation before. And the current
  // variation is also Inline.
  profile_->GetPrefs()->SetBoolean(prefs::kCartDiscountAcknowledged, false);
  profile_->GetPrefs()->SetBoolean(prefs::kCartDiscountConsentShown, true);
  profile_->GetPrefs()->SetInteger(prefs::kDiscountConsentLastShownInVariation,
                                   2);
  bool should_show = false;

  histogram_tester_.ExpectBucketCount(
      "NewTabPage.Carts.DiscountConsentStatusAtLoad.NoShowIn",
      commerce::DiscountConsentNtpVariation::kInline, 0);

  RecordDiscountConsentStatusAtLoad(should_show);
  histogram_tester_.ExpectBucketCount(
      "NewTabPage.Carts.DiscountConsentStatusAtLoad.NoShowIn",
      commerce::DiscountConsentNtpVariation::kInline, 1);

  RecordDiscountConsentStatusAtLoad(should_show);
  histogram_tester_.ExpectBucketCount(
      "NewTabPage.Carts.DiscountConsentStatusAtLoad.NoShowIn",
      commerce::DiscountConsentNtpVariation::kInline, 2);
}

TEST_F(CartServiceDiscountConsentV2Test,
       TestRecordDiscountConsentStatus_IgnoredInVariation) {
  // Simulate consent is shown in the Inline variation before, but no action has
  // been taken.
  profile_->GetPrefs()->SetBoolean(prefs::kCartDiscountAcknowledged, false);
  profile_->GetPrefs()->SetInteger(prefs::kDiscountConsentLastShownInVariation,
                                   2);
  bool should_show = true;

  histogram_tester_.ExpectBucketCount(
      "NewTabPage.Carts.DiscountConsentStatusAtLoad.IgnoredIn",
      commerce::DiscountConsentNtpVariation::kInline, 0);

  RecordDiscountConsentStatusAtLoad(should_show);
  histogram_tester_.ExpectBucketCount(
      "NewTabPage.Carts.DiscountConsentStatusAtLoad.IgnoredIn",
      commerce::DiscountConsentNtpVariation::kInline, 1);

  RecordDiscountConsentStatusAtLoad(should_show);
  histogram_tester_.ExpectBucketCount(
      "NewTabPage.Carts.DiscountConsentStatusAtLoad.IgnoredIn",
      commerce::DiscountConsentNtpVariation::kInline, 2);
}

TEST_F(CartServiceDiscountConsentV2Test,
       TestRecordDiscountConsentStatus_ShownInVariation) {
  // The current variation is Inline.
  profile_->GetPrefs()->SetBoolean(prefs::kCartDiscountAcknowledged, false);
  bool should_show = true;

  histogram_tester_.ExpectBucketCount(
      "NewTabPage.Carts.DiscountConsentStatusAtLoad.ShownIn",
      commerce::DiscountConsentNtpVariation::kInline, 0);

  RecordDiscountConsentStatusAtLoad(should_show);
  histogram_tester_.ExpectBucketCount(
      "NewTabPage.Carts.DiscountConsentStatusAtLoad.ShownIn",
      commerce::DiscountConsentNtpVariation::kInline, 1);

  RecordDiscountConsentStatusAtLoad(should_show);
  histogram_tester_.ExpectBucketCount(
      "NewTabPage.Carts.DiscountConsentStatusAtLoad.ShownIn",
      commerce::DiscountConsentNtpVariation::kInline, 2);
}

TEST_F(CartServiceDiscountConsentV2Test, TestLastShownInVariationUpdated) {
  // The current variation is Inline.
  profile_->GetPrefs()->SetBoolean(prefs::kCartDiscountAcknowledged, false);

  RecordDiscountConsentStatusAtLoad(true);
  ASSERT_EQ(2, profile_->GetPrefs()->GetInteger(
                   prefs::kDiscountConsentLastShownInVariation));
}

class CartServiceDomHeuristicsTest : public CartServiceTest {
 public:
  CartServiceDomHeuristicsTest() {
    // This needs to be called before any tasks that run on other threads check
    // if a feature is enabled.
    features_.InitWithFeaturesAndParameters(
        {{commerce::kChromeCartDomBasedHeuristics,
          {{"add-to-cart-product-image", "true"}}},
         {ntp_features::kNtpChromeCartModule, {}}},
        {});
  }
};

// Test adding a cart which has no product image in proto but has a cached image
// in ShoppingService for the URL where the addition happens.
TEST_F(CartServiceDomHeuristicsTest, TestAddCartWithCachedImage) {
  CartDB* cart_db = service_->GetDB();
  base::RunLoop run_loop[4];
  GURL product_URL_A = GURL("https://foo.com/product-A");
  GURL product_image_URL_A = GURL("https://foo.com/product-A/image");
  GURL product_image_URL_B = GURL("https://foo.com/product-B/image");

  // Set up the ShoppingService to cache a product image URL for current PDP
  // URL.
  std::optional<commerce::ProductInfo> info;
  info.emplace();
  info->image_url = product_image_URL_A;
  auto* shopping_service = static_cast<commerce::MockShoppingService*>(
      commerce::ShoppingServiceFactory::GetForBrowserContext(profile_.get()));
  shopping_service->SetResponseForGetProductInfoForUrl(std::move(info));

  // If the added proto has no image, the cached image will be added to the
  // stored proto.
  cart_db::ChromeCartContentProto proto_without_image =
      BuildProto(kMockMerchantA, kMockMerchantURLA);
  service_->AddCart(product_URL_A, std::nullopt, proto_without_image);
  task_environment_.RunUntilIdle();

  cart_db::ChromeCartContentProto proto_with_image_A = proto_without_image;
  proto_with_image_A.add_product_image_urls(product_image_URL_A.spec());
  ShoppingCarts result = {{kMockMerchantA, proto_with_image_A}};
  cart_db->LoadCart(
      kMockMerchantA,
      base::BindOnce(&CartServiceTest::GetEvaluationURL, base::Unretained(this),
                     run_loop[0].QuitClosure(), result));
  run_loop[0].Run();

  // If the stored proto already contains the cached image, it won't be added
  // again.
  service_->AddCart(product_URL_A, std::nullopt, proto_without_image);
  task_environment_.RunUntilIdle();

  cart_db->LoadCart(
      kMockMerchantA,
      base::BindOnce(&CartServiceTest::GetEvaluationURL, base::Unretained(this),
                     run_loop[1].QuitClosure(), result));
  run_loop[1].Run();

  // If the added proto has image(s), it will overwrite the existing proto and
  // the cached image URL won't be added.
  cart_db::ChromeCartContentProto proto_with_image_B = proto_without_image;
  proto_with_image_B.add_product_image_urls(product_image_URL_B.spec());
  service_->AddCart(product_URL_A, std::nullopt, proto_with_image_B);
  task_environment_.RunUntilIdle();

  result = {{kMockMerchantA, proto_with_image_B}};
  cart_db->LoadCart(
      kMockMerchantA,
      base::BindOnce(&CartServiceTest::GetEvaluationURL, base::Unretained(this),
                     run_loop[2].QuitClosure(), result));
  run_loop[2].Run();

  // If the stored proto doesn't contain the cached image, the cached image will
  // be added.
  service_->AddCart(product_URL_A, std::nullopt, proto_without_image);
  task_environment_.RunUntilIdle();

  cart_db::ChromeCartContentProto proto_with_image_A_B = proto_with_image_B;
  proto_with_image_A_B.add_product_image_urls(product_image_URL_A.spec());
  result = {{kMockMerchantA, proto_with_image_A_B}};
  cart_db->LoadCart(
      kMockMerchantA,
      base::BindOnce(&CartServiceTest::GetEvaluationURL, base::Unretained(this),
                     run_loop[3].QuitClosure(), result));
  run_loop[3].Run();
}

class CartServiceDisableDomHeuristicsTest : public CartServiceTest {
 public:
  CartServiceDisableDomHeuristicsTest() {
    // This needs to be called before any tasks that run on other threads check
    // if a feature is enabled.
    features_.InitWithFeaturesAndParameters(
        {{commerce::kChromeCartDomBasedHeuristics,
          {{"add-to-cart-product-image", "false"}}},
         {ntp_features::kNtpChromeCartModule, {}}},
        {});
  }
};

// Test adding a cart which has no product image in proto but has a cached image
// in ShoppingService for the URL where the addition happens. When the DOM-based
// heuristics feature is disabled, CartService should not try to get the image
// from ShoppingService.
TEST_F(CartServiceDisableDomHeuristicsTest, TestNoImageFromShoppingService) {
  CartDB* cart_db = service_->GetDB();
  base::RunLoop run_loop;
  GURL product_URL = GURL("https://foo.com/product-A");
  GURL product_image_URL = GURL("https://foo.com/product-A/image");

  // Set up the ShoppingService to cache a product image URL for current PDP
  // URL.
  std::optional<commerce::ProductInfo> info;
  info.emplace();
  info->image_url = product_image_URL;
  auto* shopping_service = static_cast<commerce::MockShoppingService*>(
      commerce::ShoppingServiceFactory::GetForBrowserContext(profile_.get()));
  shopping_service->SetResponseForGetProductInfoForUrl(std::move(info));

  // No getting image from ShoppingService even though there is a corresponding
  // product image.
  cart_db::ChromeCartContentProto proto_without_image =
      BuildProto(kMockMerchantA, kMockMerchantURLA);
  service_->AddCart(product_URL, std::nullopt, proto_without_image);
  task_environment_.RunUntilIdle();

  ShoppingCarts result = {{kMockMerchantA, proto_without_image}};
  cart_db->LoadCart(
      kMockMerchantA,
      base::BindOnce(&CartServiceTest::GetEvaluationURL, base::Unretained(this),
                     run_loop.QuitClosure(), result));
  run_loop.Run();
}
