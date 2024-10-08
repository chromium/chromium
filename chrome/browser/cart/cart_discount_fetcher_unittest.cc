// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cart/cart_discount_fetcher.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/cart/fetch_discount_worker.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/endpoint_fetcher/mock_endpoint_fetcher.h"
#include "components/search/ntp_features.h"
#include "components/session_proto_db/session_proto_db.h"
#include "content/public/test/browser_task_environment.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
cart_db::ChromeCartContentProto BuildProto(
    const char* domain,
    const char* merchant_url,
    const double timestamp,
    const std::vector<std::string>& raw_merchant_offers) {
  cart_db::ChromeCartContentProto proto;
  proto.set_key(domain);
  proto.set_merchant_cart_url(merchant_url);
  proto.set_timestamp(timestamp);
  for (const std::string& offer : raw_merchant_offers) {
    proto.add_product_infos()->set_product_id(offer);
  }
  return proto;
}
using ShoppingCarts =
    std::vector<SessionProtoDB<cart_db::ChromeCartContentProto>::KeyAndValue>;

const char kMockMerchantA[] = "foo.com";
const char kMockMerchantCartURLA[] = "https://www.foo.com/cart";
const char kMockMerchantAHighestDiscountString[] = "Up to $10 off";
const int kMockMerchantALastTimestamp = 1;
const int kThirtyMinutesInSeconds = 1800;
const char kMockMerchantARawOfferIdOne[] = "offerId1";
const char kMockMerchantARawOfferIdTwo[] = "offerId2";
const char kConfigurableEndpoint[] = "https://testing_endpoint.com/discounts";
const char kLocales[] = "en-US";
const char kVariationHeaders[] = "variationsHeaders";

const char kEndpointResponse[] =
    "{ "
    "  \"discounts\": [ "
    "    { "
    "      \"merchantIdentifier\": { "
    "        \"cartUrl\": \"https://www.foo.com/cart\", "
    "        \"merchantId\": \"0\" "
    "      }, "
    "      \"ruleDiscounts\": [ "
    "        { "
    "          \"ruleId\": \"0\", "
    "          \"discount\": { "
    "            \"percentOff\": 10 "
    "            }, "
    "          \"merchantRuleId\": \"1\", "
    "          \"rawMerchantOfferId\": \"offerId1\"  "
    "        }, "
    "        { "
    "          \"ruleId\": \"1\", "
    "          \"discount\": { "
    "            \"amountOff\": { "
    "              \"currencyCode\": \"USD\", "
    "              \"units\": \"10\", "
    "              \"nanos\": 0 "
    "            } "
    "          }, "
    "          \"merchantRuleId\": \"1\", "
    "          \"rawMerchantOfferId\": \"offerId2\"  "
    "        } "
    "      ] "
    "    } "
    "  ] "
    "} ";

}  // namespace

class CartDiscountFetcherTest {
 public:
  static std::string generatePostData(
      std::vector<CartDB::KeyAndValue> proto_pairs,
      double current_timestamp) {
    return CartDiscountFetcher::generatePostData(
        std::move(proto_pairs),
        base::Time::FromSecondsSinceUnixEpoch(current_timestamp));
  }

  static std::unique_ptr<EndpointFetcher> CreateEndpointFetcher(
      std::unique_ptr<network::PendingSharedURLLoaderFactory> pending_factory,
      std::vector<CartDB::KeyAndValue> proto_pairs,
      bool is_oauth_fetch,
      std::string fetch_for_locale) {
    return CartDiscountFetcher::CreateEndpointFetcher(
        std::move(pending_factory), std::move(proto_pairs), is_oauth_fetch,
        std::move(fetch_for_locale), kVariationHeaders);
  }

  static void OnDiscountsAvailable(
      std::unique_ptr<EndpointFetcher> endpoint_fetcher,
      CartDiscountFetcher::CartDiscountFetcherCallback callback,
      std::unique_ptr<EndpointResponse> responses) {
    CartDiscountFetcher::OnDiscountsAvailable(
        std::move(endpoint_fetcher), std::move(callback), std::move(responses));
  }
};

TEST(CartDiscountFetcherTest, TestGeneratePostData) {
  ShoppingCarts shoppingcartA = {
      {kMockMerchantA,
       BuildProto(kMockMerchantA, kMockMerchantCartURLA,
                  kMockMerchantALastTimestamp,
                  {kMockMerchantARawOfferIdOne, kMockMerchantARawOfferIdTwo})}};

  // Expected json string:
  // "{"
  // "  \"carts\": ["
  // "    {"
  // "      \"cartAbandonedTimeMinutes\": 30,"
  // "      \"merchantIdentifier\": {"
  // "        \"cartUrl\": \"www.foo.com/cart\","
  // "      },"
  // "      \"rawMerchantOffers\": ["
  // "        \"offerId1\","
  // "        \"offerId2\}"
  // "      ]"
  // "    }"
  // "  ]"
  // "}"
  std::string expected =
      "{\"carts\":[{\"cartAbandonedTimeMinutes\":30,\"merchantIdentifier\":{"
      "\"cartUrl\":\"https://www.foo.com/"
      "cart\"},\"rawMerchantOffers\":[\"offerId1\",\"offerId2\"]}]}";

  int abandondTimeInSeconds =
      kMockMerchantALastTimestamp + kThirtyMinutesInSeconds;  // 30 Minutes
  std::string result = CartDiscountFetcherTest::generatePostData(
      std::move(shoppingcartA), abandondTimeInSeconds);
  EXPECT_EQ(expected, result);
}

TEST(CartDiscountFetcherTest, TestOnDiscountsAvailableParsing) {
  std::unique_ptr<EndpointFetcher> mock_endpoint_fetcher =
      std::make_unique<MockEndpointFetcher>();

  base::MockCallback<CartDiscountFetcher::CartDiscountFetcherCallback>
      mock_callback;

  auto fake_responses = std::make_unique<EndpointResponse>();
  fake_responses->response = kEndpointResponse;

  // TODO(meiliang): Test the callback argument.
  EXPECT_CALL(mock_callback, Run(testing::_, testing::_));

  CartDiscountFetcherTest::OnDiscountsAvailable(
      std::move(mock_endpoint_fetcher), mock_callback.Get(),
      std::move(fake_responses));
}

TEST(CartDiscountFetcherTest, TestHighestDiscounts) {
  std::unique_ptr<EndpointFetcher> mock_endpoint_fetcher =
      std::make_unique<MockEndpointFetcher>();

  base::MockCallback<CartDiscountFetcher::CartDiscountFetcherCallback>
      mock_callback;

  auto fake_responses = std::make_unique<EndpointResponse>();
  fake_responses->response = kEndpointResponse;

  CartDiscountFetcher::CartDiscountMap cart_discount_map;
  EXPECT_CALL(mock_callback, Run(testing::_, testing::_))
      .WillOnce(testing::SaveArg<0>(&cart_discount_map));

  CartDiscountFetcherTest::OnDiscountsAvailable(
      std::move(mock_endpoint_fetcher), mock_callback.Get(),
      std::move(fake_responses));

  EXPECT_EQ(cart_discount_map.size(), 1u);

  EXPECT_EQ(cart_discount_map.at(kMockMerchantCartURLA).highest_discount_string,
            kMockMerchantAHighestDiscountString);
}

TEST(CartDiscountFetcherTest, TestRawMaerchantOffersIsOptional) {
  std::unique_ptr<EndpointFetcher> mock_endpoint_fetcher =
      std::make_unique<MockEndpointFetcher>();

  base::MockCallback<CartDiscountFetcher::CartDiscountFetcherCallback>
      mock_callback;

  auto fake_responses = std::make_unique<EndpointResponse>();
  fake_responses->response =
      "{ "
      "  \"discounts\": [ "
      "    { "
      "      \"merchantIdentifier\": { "
      "        \"cartUrl\": \"https://www.foo.com/cart\", "
      "        \"merchantId\": \"0\" "
      "      }, "
      "      \"ruleDiscounts\": [ "
      "        { "
      "          \"ruleId\": \"0\", "
      "          \"discount\": { "
      "            \"percentOff\": 10 "
      "            }, "
      "          \"merchantRuleId\": \"1\" "
      "        } "
      "      ] "
      "    } "
      "  ] "
      "} ";
  const bool expected_not_a_tester = false;
  CartDiscountFetcher::CartDiscountMap cart_discount_map;
  EXPECT_CALL(mock_callback, Run(testing::_, expected_not_a_tester))
      .WillOnce(testing::SaveArg<0>(&cart_discount_map));

  CartDiscountFetcherTest::OnDiscountsAvailable(
      std::move(mock_endpoint_fetcher), mock_callback.Get(),
      std::move(fake_responses));

  EXPECT_EQ(cart_discount_map.size(), 1u);
  EXPECT_EQ(cart_discount_map.at(kMockMerchantCartURLA).rule_discounts.size(),
            1u);
  EXPECT_TRUE(cart_discount_map.at(kMockMerchantCartURLA)
                  .rule_discounts[0]
                  .raw_merchant_offer_id()
                  .empty());
}

TEST(CartDiscountFetcherTest, TestExternalTesterDiscount) {
  std::unique_ptr<EndpointFetcher> mock_endpoint_fetcher =
      std::make_unique<MockEndpointFetcher>();

  base::MockCallback<CartDiscountFetcher::CartDiscountFetcherCallback>
      mock_callback;

  auto fake_responses = std::make_unique<EndpointResponse>();
  fake_responses->response =
      "{ "
      "  \"discounts\": [ "
      "    { "
      "      \"merchantIdentifier\": { "
      "        \"cartUrl\": \"https://www.foo.com/cart\", "
      "        \"merchantId\": \"0\" "
      "      }, "
      "      \"ruleDiscounts\": [ "
      "        { "
      "          \"ruleId\": \"0\", "
      "          \"discount\": { "
      "            \"percentOff\": 10 "
      "            }, "
      "          \"merchantRuleId\": \"1\" "
      "        } "
      "      ] "
      "    } "
      "  ], "
      "  \"externalTester\": true "
      "} ";

  const bool expected_a_tester = true;
  CartDiscountFetcher::CartDiscountMap cart_discount_map;
  EXPECT_CALL(mock_callback, Run(testing::_, expected_a_tester))
      .WillOnce(testing::SaveArg<0>(&cart_discount_map));

  CartDiscountFetcherTest::OnDiscountsAvailable(
      std::move(mock_endpoint_fetcher), mock_callback.Get(),
      std::move(fake_responses));

  EXPECT_EQ(cart_discount_map.size(), 1u);
  EXPECT_EQ(cart_discount_map.at(kMockMerchantCartURLA).rule_discounts.size(),
            1u);
  EXPECT_TRUE(cart_discount_map.at(kMockMerchantCartURLA)
                  .rule_discounts[0]
                  .raw_merchant_offer_id()
                  .empty());
}

TEST(CartDiscountFetcherTest, TestNoRuleDiscounts) {
  std::unique_ptr<EndpointFetcher> mock_endpoint_fetcher =
      std::make_unique<MockEndpointFetcher>();

  base::MockCallback<CartDiscountFetcher::CartDiscountFetcherCallback>
      mock_callback;

  auto fake_responses = std::make_unique<EndpointResponse>();
  fake_responses->response = R"(
    {
      "discounts": [
        {
          "merchantIdentifier": {
            "cartUrl": "https://www.foo.com/cart",
            "merchantId": "0"
          }
        }
      ]
    }
  )";

  CartDiscountFetcher::CartDiscountMap cart_discount_map;
  EXPECT_CALL(mock_callback, Run(testing::_, testing::_))
      .WillOnce(testing::SaveArg<0>(&cart_discount_map));

  CartDiscountFetcherTest::OnDiscountsAvailable(
      std::move(mock_endpoint_fetcher), mock_callback.Get(),
      std::move(fake_responses));

  EXPECT_EQ(cart_discount_map.size(), 1u);
  EXPECT_EQ(cart_discount_map.at(kMockMerchantCartURLA).rule_discounts.size(),
            0u);
}

TEST(CartDiscountFetcherTest, TestOverallDiscountText) {
  std::unique_ptr<EndpointFetcher> mock_endpoint_fetcher =
      std::make_unique<MockEndpointFetcher>();

  base::MockCallback<CartDiscountFetcher::CartDiscountFetcherCallback>
      mock_callback;

  auto fake_responses = std::make_unique<EndpointResponse>();
  fake_responses->response = R"(
    {
      "discounts": [
        {
          "merchantIdentifier": {
            "cartUrl": "https://www.foo.com/cart",
            "merchantId": "0"
          },
          "overallDiscountInfo": {
             "text": "10% off"
          }
        }
      ]
    }
  )";

  CartDiscountFetcher::CartDiscountMap cart_discount_map;
  EXPECT_CALL(mock_callback, Run(testing::_, testing::_))
      .WillOnce(testing::SaveArg<0>(&cart_discount_map));

  CartDiscountFetcherTest::OnDiscountsAvailable(
      std::move(mock_endpoint_fetcher), mock_callback.Get(),
      std::move(fake_responses));

  EXPECT_EQ(cart_discount_map.size(), 1u);
  EXPECT_EQ(cart_discount_map.at(kMockMerchantCartURLA).highest_discount_string,
            "10% off");
}

TEST(CartDiscountFetcherTest, TestOverallDiscountTextWithRuleDiscounts) {
  std::unique_ptr<EndpointFetcher> mock_endpoint_fetcher =
      std::make_unique<MockEndpointFetcher>();

  base::MockCallback<CartDiscountFetcher::CartDiscountFetcherCallback>
      mock_callback;

  auto fake_responses = std::make_unique<EndpointResponse>();

  fake_responses->response = R"(
    {
      "discounts": [
        {
          "merchantIdentifier": {
            "cartUrl": "https://www.foo.com/cart",
            "merchantId": "0"
          },
          "overallDiscountInfo": {
             "text": "20% off"
          },
          "ruleDiscounts": [
            {
              "ruleId": "0",
              "discount": {
                "percentOff": 10
                },
              "merchantRuleId": "1"
            }
          ]
        }
      ]
    }
  )";

  CartDiscountFetcher::CartDiscountMap cart_discount_map;
  EXPECT_CALL(mock_callback, Run(testing::_, testing::_))
      .WillOnce(testing::SaveArg<0>(&cart_discount_map));

  CartDiscountFetcherTest::OnDiscountsAvailable(
      std::move(mock_endpoint_fetcher), mock_callback.Get(),
      std::move(fake_responses));

  EXPECT_EQ(cart_discount_map.size(), 1u);
  EXPECT_EQ(cart_discount_map.at(kMockMerchantCartURLA).highest_discount_string,
            "20% off");
}

TEST(CartDiscountFetcherTest, TestCodeBasedRuleDiscount) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{commerce::kCodeBasedRBD,
        {{commerce::kCodeBasedRuleDiscountParam, "true"}}}},
      {});

  std::unique_ptr<EndpointFetcher> mock_endpoint_fetcher =
      std::make_unique<MockEndpointFetcher>();

  base::MockCallback<CartDiscountFetcher::CartDiscountFetcherCallback>
      mock_callback;

  auto fake_responses = std::make_unique<EndpointResponse>();

  fake_responses->response = R"(
    {
      "discounts": [
        {
          "merchantIdentifier": {
            "cartUrl": "https://www.foo.com/cart",
            "merchantId": "0"
          },
          "couponDiscounts": [
            {
              "type": "RBD_WITH_CODE",
              "description": {
                "title": "Save $10 on Running shoes.",
                "languageCode": "en-US"
              },
              "couponCode": "SAVE$10",
              "couponId": "1",
              "expiryTimeSec": 1635204292
            }
          ]
        }
      ]
    }
  )";

  CartDiscountFetcher::CartDiscountMap cart_discount_map;
  EXPECT_CALL(mock_callback, Run(testing::_, testing::_))
      .WillOnce(testing::SaveArg<0>(&cart_discount_map));

  CartDiscountFetcherTest::OnDiscountsAvailable(
      std::move(mock_endpoint_fetcher), mock_callback.Get(),
      std::move(fake_responses));

  ASSERT_EQ(cart_discount_map.size(), 1u);
  EXPECT_EQ(cart_discount_map.at(kMockMerchantCartURLA).rule_discounts.size(),
            0u);
  EXPECT_EQ(cart_discount_map.at(kMockMerchantCartURLA).coupon_discounts.size(),
            1u);
  EXPECT_TRUE(cart_discount_map.at(kMockMerchantCartURLA).has_coupons);
}

class CartDiscountFetcherConfigurableEndpointTest : public testing::Test {
 public:
  // Features need to be initialized before CartServiceTest::SetUp runs, in
  // order to avoid tsan data race error on FeatureList.
  CartDiscountFetcherConfigurableEndpointTest() {
    features_.InitAndEnableFeatureWithParameters(
        ntp_features::kNtpChromeCartModule,
        {{"CartDiscountFetcherEndpointParam", kConfigurableEndpoint}});
  }

 protected:
  base::test::ScopedFeatureList features_;
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(CartDiscountFetcherConfigurableEndpointTest,
       TestServerConfiguraleEndpoint) {
  scoped_refptr<network::TestSharedURLLoaderFactory> shared_url_loader_factory =
      base::MakeRefCounted<network::TestSharedURLLoaderFactory>();
  std::unique_ptr<EndpointFetcher> endpoint_fetcher =
      CartDiscountFetcherTest::CreateEndpointFetcher(
          shared_url_loader_factory->Clone(), {}, false /*is_oauth_fetch*/,
          kLocales);
  EXPECT_EQ(endpoint_fetcher->GetUrlForTesting(), kConfigurableEndpoint);
}
