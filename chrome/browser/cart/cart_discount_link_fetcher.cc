// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cart/cart_discount_link_fetcher.h"

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "chrome/browser/cart/cart_db.h"
#include "chrome/browser/cart/cart_discount_metric_collector.h"
#include "components/commerce/core/proto/cart_db_content.pb.h"
#include "components/endpoint_fetcher/endpoint_fetcher.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/cross_thread_pending_shared_url_loader_factory.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace {
const char kPostMethod[] = "POST";
const char kContentType[] = "application/json; charset=UTF-8";

const char kFetchDiscountLinkEndpoint[] =
    "https://memex-pa.googleapis.com/v1/shopping/cart/discounted";
constexpr base::TimeDelta kTimeout = base::Milliseconds(30000);

static std::string GeneratePostData(
    cart_db::ChromeCartContentProto cart_content_proto) {
  const cart_db::ChromeCartDiscountProto& cart_discount_proto =
      cart_content_proto.discount_info();

  base::Value::Dict discount_dict;
  // MerchantIdentifier
  base::Value::Dict merchant_identifier_dict;
  merchant_identifier_dict.Set("cartUrl",
                               cart_content_proto.merchant_cart_url());
  merchant_identifier_dict.Set("merchantId", cart_discount_proto.merchant_id());
  discount_dict.Set("merchantIdentifier", std::move(merchant_identifier_dict));

  // RuleDiscount
  if (!cart_discount_proto.rule_discount_info_size()) {
    NOTREACHED_IN_MIGRATION() << "discount_info should not be empty";
  }
  base::Value::List rule_discounts_list;
  for (int i = 0; i < cart_discount_proto.rule_discount_info_size(); i++) {
    const cart_db::RuleDiscountInfoProto& rule_discount_info_proto =
        cart_discount_proto.rule_discount_info(i);

    base::Value::Dict rule_discount;
    // ruleId
    rule_discount.Set("ruleId", rule_discount_info_proto.rule_id());
    // merchanRuletId
    rule_discount.Set("merchantRuleId",
                      rule_discount_info_proto.merchant_rule_id());
    // rawMerchantOfferId
    if (!rule_discount_info_proto.raw_merchant_offer_id().empty()) {
      rule_discount.Set("rawMerchantOfferId",
                        rule_discount_info_proto.raw_merchant_offer_id());
    }
    // discount
    base::Value::Dict discount;
    if (rule_discount_info_proto.has_amount_off()) {
      base::Value::Dict money;

      const cart_db::MoneyProto& money_proto =
          rule_discount_info_proto.amount_off();
      money.Set("currencyCode", money_proto.currency_code());
      money.Set("units", money_proto.units());
      money.Set("nanos", money_proto.nanos());

      discount.Set("amountOff", std::move(money));
    } else {
      discount.Set("percentOff", rule_discount_info_proto.percent_off());
    }
    rule_discount.Set("discount", std::move(discount));

    rule_discounts_list.Append(std::move(rule_discount));
  }
  discount_dict.Set("ruleDiscounts", std::move(rule_discounts_list));

  base::Value::Dict request_dict;
  request_dict.Set("discount", std::move(discount_dict));
  request_dict.Set("baseUrl", cart_content_proto.merchant_cart_url());
  request_dict.Set("merchantId", cart_discount_proto.merchant_id());

  std::string request_json;
  base::JSONWriter::Write(request_dict, &request_json);
  VLOG(2) << "Request body: " << request_json;
  return request_json;
}

static std::unique_ptr<EndpointFetcher> CreateEndpointFetcher(
    std::unique_ptr<network::PendingSharedURLLoaderFactory> pending_factory,
    cart_db::ChromeCartContentProto cart_content_proto) {
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("chrome_cart_get_discounted_link", R"(
        semantics {
          sender: "Chrome Cart"
          description:
            "Chrome gets a discounted link for the given 'Chrome Shopping Cart'"
            " that has the discount info."
          trigger:
            "After user has clicked on the 'Chrome Shopping Cart' that has a "
            "discount label on the New Tab Page."
          data:
            "The Chrome Cart data, includes the shopping site and merchant "
            "discount info."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "You can enable or disable this feature via the Chrome NTP "
            "customized page."
          policy_exception_justification: "No policy provided because this "
            "does not require user to sign in or sync, and they must given "
            "their consent before triggering this. And user can disable this "
            "feature."
        })");

  const std::vector<std::string> empty_header = {};

  std::string post_data = GeneratePostData(std::move(cart_content_proto));
  return std::make_unique<EndpointFetcher>(
      GURL(kFetchDiscountLinkEndpoint), kPostMethod, kContentType, kTimeout,
      post_data, empty_header, empty_header, traffic_annotation,
      network::SharedURLLoaderFactory::Create(std::move(pending_factory)),
      false);
}

static void OnLinkFetched(
    std::unique_ptr<EndpointFetcher> endpoint_fetcher,
    CartDiscountLinkFetcher::CartDiscountLinkFetcherCallback callback,
    std::unique_ptr<EndpointResponse> responses) {
  VLOG(2) << "Response: " << responses->response;
  DCHECK(responses) << "responses should not be null";
  if (!responses) {
    std::move(callback).Run(GURL());
    return;
  }

  std::optional<base::Value> value =
      base::JSONReader::Read(responses->response);

  if (!value || !value->is_dict() || !value->GetDict().FindString("url")) {
    NOTREACHED_IN_MIGRATION() << "empty response or wrong format";
    std::move(callback).Run(GURL());
    return;
  }
  std::move(callback).Run(GURL(*value->GetDict().FindString("url")));
}
}  // namespace

CartDiscountLinkFetcher::~CartDiscountLinkFetcher() = default;

void CartDiscountLinkFetcher::Fetch(
    std::unique_ptr<network::PendingSharedURLLoaderFactory> pending_factory,
    cart_db::ChromeCartContentProto cart_content_proto,
    CartDiscountLinkFetcherCallback callback) {
  auto fetcher = CreateEndpointFetcher(std::move(pending_factory),
                                       std::move(cart_content_proto));

  auto* const fetcher_ptr = fetcher.get();
  fetcher_ptr->PerformRequest(
      base::BindOnce(&OnLinkFetched, std::move(fetcher), std::move(callback)),
      nullptr);
  CartDiscountMetricCollector::RecordFetchingForDiscountedLink();
}

std::string CartDiscountLinkFetcher::GeneratePostDataForTesting(
    cart_db::ChromeCartContentProto cart_content_proto) {
  return GeneratePostData(cart_content_proto);
}
