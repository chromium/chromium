// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cart/cart_discount_link_fetcher.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
cart_db::ChromeCartContentProto BuildProto(
    const std::string& domain,
    const std::string& merchant_url,
    const std::string& merchant_id,
    const double last_used_timestamp,
    const std::string& rule_id,
    const std::string& merchant_rule_id,
    const std::string& raw_merchant_offer_id) {
  cart_db::ChromeCartContentProto proto;
  proto.set_key(domain);
  proto.set_merchant_cart_url(merchant_url);
  proto.set_timestamp(last_used_timestamp);
  proto.mutable_discount_info()->set_merchant_id(merchant_id);
  cart_db::RuleDiscountInfoProto* discount =
      proto.mutable_discount_info()->add_rule_discount_info();
  discount->set_rule_id(rule_id);
  discount->set_merchant_rule_id(merchant_rule_id);
  discount->set_raw_merchant_offer_id(raw_merchant_offer_id);
  return proto;
}

cart_db::ChromeCartContentProto BuildProtoWithPercentOff(
    const std::string& domain,
    const std::string& merchant_url,
    const std::string& merchant_id,
    const double last_used_timestamp,
    const std::string& rule_id,
    const std::string& merchant_rule_id,
    const std::string& raw_merchant_offer_id,
    const int percent_off) {
  auto proto =
      BuildProto(domain, merchant_url, merchant_id, last_used_timestamp,
                 rule_id, merchant_rule_id, raw_merchant_offer_id);
  cart_db::RuleDiscountInfoProto* discount =
      proto.mutable_discount_info()->mutable_rule_discount_info(0);
  discount->set_percent_off(percent_off);

  return proto;
}

cart_db::ChromeCartContentProto BuildProtoWithAmountOff(
    const std::string& domain,
    const std::string& merchant_url,
    const std::string& merchant_id,
    const double last_used_timestamp,
    const std::string& rule_id,
    const std::string& merchant_rule_id,
    const std::string& raw_merchant_offer_id,
    const std::string& currency_code,
    const std::string& units,
    const int nanos) {
  auto proto =
      BuildProto(domain, merchant_url, merchant_id, last_used_timestamp,
                 rule_id, merchant_rule_id, raw_merchant_offer_id);
  cart_db::RuleDiscountInfoProto* discount =
      proto.mutable_discount_info()->mutable_rule_discount_info(0);

  cart_db::MoneyProto* money = discount->mutable_amount_off();
  money->set_currency_code(currency_code);
  money->set_units(units);
  money->set_nanos(nanos);

  return proto;
}

const char kMockMerchantA[] = "foo.com";
const char kMockMerchantACartURL[] = "https://www.foo.com/cart";
const int kMockMerchantALastTimestamp = 1;
const char kMockMerchantAId[] = "123";
const char kMockMerchantARuleId[] = "456";
const char kMockMerchantARawMerchantOfferId[] = "789";
const int kMockMerchantAPercentOff = 10;
const char kMockMerchantAMoneyOffCurrency[] = "USD";
const char kMockMerchantAMonneyOffUnits[] = "10";
const int kMockMerchantAMonneyOffNanos = 0;
}  // namespace

class CartDiscountLinkFetcherTest {
 public:
  static std::string generatePostData(
      cart_db::ChromeCartContentProto cart_content_proto) {
    return CartDiscountLinkFetcher::GeneratePostDataForTesting(
        std::move(cart_content_proto));
  }
};

TEST(CartDiscountLinkFetcherTest, TestGeneratePostData_PercentOff) {
  cart_db::ChromeCartContentProto cart_content_proto = BuildProtoWithPercentOff(
      kMockMerchantA, kMockMerchantACartURL, kMockMerchantAId,
      kMockMerchantALastTimestamp, kMockMerchantARuleId, kMockMerchantARuleId,
      kMockMerchantARawMerchantOfferId, kMockMerchantAPercentOff);

  std::string expected =
      "{\"baseUrl\":\"https://www.foo.com/"
      "cart\",\"discount\":{\"merchantIdentifier\":{\"cartUrl\":\"https://"
      "www.foo.com/"
      "cart\",\"merchantId\":\"123\"},\"ruleDiscounts\":[{\"discount\":{"
      "\"percentOff\":10},\"merchantRuleId\":\"456\",\"rawMerchantOfferId\":"
      "\"789\",\"ruleId\":\"456\"}]},\"merchantId\":\"123\"}";

  std::string generated_post_data =
      CartDiscountLinkFetcherTest::generatePostData(
          std::move(cart_content_proto));
  EXPECT_EQ(expected, generated_post_data);
}

TEST(CartDiscountLinkFetcherTest, TestGeneratePostData_AmountOff) {
  cart_db::ChromeCartContentProto cart_content_proto = BuildProtoWithAmountOff(
      kMockMerchantA, kMockMerchantACartURL, kMockMerchantAId,
      kMockMerchantALastTimestamp, kMockMerchantARuleId, kMockMerchantARuleId,
      kMockMerchantARawMerchantOfferId, kMockMerchantAMoneyOffCurrency,
      kMockMerchantAMonneyOffUnits, kMockMerchantAMonneyOffNanos);

  std::string expected =
      "{\"baseUrl\":\"https://www.foo.com/"
      "cart\",\"discount\":{\"merchantIdentifier\":{\"cartUrl\":\"https://"
      "www.foo.com/"
      "cart\",\"merchantId\":\"123\"},\"ruleDiscounts\":[{\"discount\":{"
      "\"amountOff\":{\"currencyCode\":\"USD\",\"nanos\":0,\"units\":\"10\"}},"
      "\"merchantRuleId\":\"456\",\"rawMerchantOfferId\":\"789\",\"ruleId\":"
      "\"456\"}]},\"merchantId\":\"123\"}";

  std::string generated_post_data =
      CartDiscountLinkFetcherTest::generatePostData(
          std::move(cart_content_proto));
  EXPECT_EQ(expected, generated_post_data);
}

TEST(CartDiscountLinkFetcherTest,
     TestGeneratePostData_OptionalRawMerchantOfferId) {
  cart_db::ChromeCartContentProto cart_content_proto = BuildProtoWithAmountOff(
      kMockMerchantA, kMockMerchantACartURL, kMockMerchantAId,
      kMockMerchantALastTimestamp, kMockMerchantARuleId, kMockMerchantARuleId,
      "", kMockMerchantAMoneyOffCurrency, kMockMerchantAMonneyOffUnits,
      kMockMerchantAMonneyOffNanos);

  std::string expected =
      "{\"baseUrl\":\"https://www.foo.com/"
      "cart\",\"discount\":{\"merchantIdentifier\":{\"cartUrl\":\"https://"
      "www.foo.com/"
      "cart\",\"merchantId\":\"123\"},\"ruleDiscounts\":[{\"discount\":{"
      "\"amountOff\":{\"currencyCode\":\"USD\",\"nanos\":0,\"units\":\"10\"}},"
      "\"merchantRuleId\":\"456\",\"ruleId\":"
      "\"456\"}]},\"merchantId\":\"123\"}";

  std::string generated_post_data =
      CartDiscountLinkFetcherTest::generatePostData(
          std::move(cart_content_proto));
  EXPECT_EQ(expected, generated_post_data);
}
