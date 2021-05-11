// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cart/cart_discount_fetcher.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/cart/fetch_discount_worker.h"
#include "chrome/browser/endpoint_fetcher/endpoint_fetcher.h"
#include "chrome/browser/persisted_state_db/profile_proto_db.h"
#include "content/public/test/browser_task_environment.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
cart_db::ChromeCartContentProto BuildProto(const char* domain,
                                           const char* merchant_url,
                                           const double timestamp) {
  cart_db::ChromeCartContentProto proto;
  proto.set_key(domain);
  proto.set_merchant_cart_url(merchant_url);
  proto.set_timestamp(timestamp);
  return proto;
}
using ShoppingCarts =
    std::vector<ProfileProtoDB<cart_db::ChromeCartContentProto>::KeyAndValue>;

const char kMockMerchantA[] = "foo.com";
const char kMockMerchantCartURLA[] = "https://www.foo.com/cart";
const int kMockMerchantALastTimestamp = 1;
const int kThirtyMinutesInSeconds = 1800;

const char kEndpointResponse[] =
    "{ "
    "  \"discounts\": [ "
    "    { "
    "      \"merchantIdentifier\": { "
    "        \"cartUrl\": \"http://foo.com/cart\", "
    "        \"merchantId\": \"0\" "
    "      }, "
    "      \"ruleDiscounts\": [ "
    "        { "
    "          \"ruleId\": \"0\", "
    "          \"discount\": { "
    "            \"percentOff\": 99 "
    "            }, "
    "          \"merchantRuleId\": \"1\", "
    "          \"rawMerchantOfferId\": \"offerID\"  "
    "        }, "
    "        { "
    "          \"ruleId\": \"1\", "
    "          \"discount\": { "
    "            \"amountOff\": { "
    "              \"currencyCode\": \"USD\", "
    "              \"units\": \"2\", "
    "              \"nanos\": 1 "
    "            } "
    "          }, "
    "          \"merchantRuleId\": \"1\", "
    "          \"rawMerchantOfferId\": \"offerID\"  "
    "        } "
    "      ] "
    "    } "
    "  ] "
    "} ";

}  // namespace

class MockEndpointFetcher : public EndpointFetcher {
 public:
  explicit MockEndpointFetcher(
      const net::NetworkTrafficAnnotationTag& annotation_tag)
      : EndpointFetcher(annotation_tag) {}

  MOCK_METHOD(void,
              PerformRequest,
              (EndpointFetcherCallback endpoint_fetcher_callback,
               const char* key),
              (override));
};

class CartDiscountFetcherTest {
 public:
  static std::string generatePostData(
      std::vector<CartDB::KeyAndValue> proto_pairs,
      double current_timestamp) {
    return CartDiscountFetcher::generatePostData(
        std::move(proto_pairs), base::Time::FromDoubleT(current_timestamp));
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
      {kMockMerchantA, BuildProto(kMockMerchantA, kMockMerchantCartURLA,
                                  kMockMerchantALastTimestamp)}};

  // Expected json string:
  // "{"
  // "  \"carts\": ["
  // "    {"
  // "      \"cartAbandonedTimeMinutes\": 30,"
  // "      \"merchantIdentifier\": {"
  // "        \"cartUrl\": \"www.foo.com/cart\","
  // "      },"
  // "      \"rawMerchantOffers\": ["
  // "        \"offer1\","
  // "        \"offer2\}"
  // "      ]"
  // "    }"
  // "  ]"
  // "}"
  std::string expected =
      "{\"carts\":[{\"cartAbandonedTimeMinutes\":30,\"merchantIdentifier\":{"
      "\"cartUrl\":\"https://www.foo.com/"
      "cart\"},\"rawMerchantOffers\":[\"offer1\",\"offer2\"]}]}";

  int abandondTimeInSeconds =
      kMockMerchantALastTimestamp + kThirtyMinutesInSeconds;  // 30 Minutes
  std::string result = CartDiscountFetcherTest::generatePostData(
      std::move(shoppingcartA), abandondTimeInSeconds);
  EXPECT_EQ(expected, result);
}

TEST(CartDiscountFetcherTest, TestOnDiscountsAvailableParsing) {
  std::unique_ptr<EndpointFetcher> mock_endpoint_fetcher =
      std::make_unique<MockEndpointFetcher>(TRAFFIC_ANNOTATION_FOR_TESTS);

  base::MockCallback<CartDiscountFetcher::CartDiscountFetcherCallback>
      mock_callback;

  auto fake_responses = std::make_unique<EndpointResponse>();
  fake_responses->response = kEndpointResponse;

  // TODO(meiliang): Test the callback argument.
  EXPECT_CALL(mock_callback, Run(testing::_));

  CartDiscountFetcherTest::OnDiscountsAvailable(
      std::move(mock_endpoint_fetcher), mock_callback.Get(),
      std::move(fake_responses));
}
