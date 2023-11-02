// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cart/cart_discount_link_fetcher.h"

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "chrome/browser/cart/cart_db.h"
#include "chrome/browser/cart/cart_discount_metric_collector.h"
#include "chrome/browser/profiles/profile.h"
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
const int64_t kTimeoutMs = 30000;
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

std::unique_ptr<EndpointFetcher> CartDiscountLinkFetcher::CreateEndpointFetcher(
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
      GURL(kFetchDiscountLinkEndpoint), kPostMethod, kContentType, kTimeoutMs,
      post_data, empty_header, empty_header, traffic_annotation,
      network::SharedURLLoaderFactory::Create(std::move(pending_factory)),
      false);
}

std::string CartDiscountLinkFetcher::GeneratePostData(
    cart_db::ChromeCartContentProto cart_content_proto) {
  const cart_db::ChromeCartDiscountProto& cart_discount_proto =
      cart_content_proto.discount_info();

  base::Value discount_dict(base::Value::Type::DICTIONARY);
  // MerchantIdentifier
  base::Value merchant_identifier_dict(base::Value::Type::DICTIONARY);
  merchant_identifier_dict.SetStringKey("cartUrl",
                                        cart_content_proto.merchant_cart_url());
  merchant_identifier_dict.SetStringKey("merchantId",
                                        cart_discount_proto.merchant_id());
  discount_dict.SetKey("merchantIdentifier",
                       std::move(merchant_identifier_dict));

  // RuleDiscount
  if (!cart_discount_proto.rule_discount_info_size()) {
    NOTREACHED() << "discount_info should not be empty";
  }
  base::Value rule_discounts_list(base::Value::Type::LIST);
  for (int i = 0; i < cart_discount_proto.rule_discount_info_size(); i++) {
    const cart_db::RuleDiscountInfoProto& rule_discount_info_proto =
        cart_discount_proto.rule_discount_info(i);

    base::Value rule_discount(base::Value::Type::DICTIONARY);
    // ruleId
    rule_discount.SetStringKey("ruleId", rule_discount_info_proto.rule_id());
    // merchanRuletId
    rule_discount.SetStringKey("merchantRuleId",
                               rule_discount_info_proto.merchant_rule_id());
    // rawMerchantOfferId
    if (!rule_discount_info_proto.raw_merchant_offer_id().empty()) {
      rule_discount.SetStringKey(
          "rawMerchantOfferId",
          rule_discount_info_proto.raw_merchant_offer_id());
    }
    // discount
    base::Value discount(base::Value::Type::DICTIONARY);
    if (rule_discount_info_proto.has_amount_off()) {
      base::Value money(base::Value::Type::DICTIONARY);

      const cart_db::MoneyProto& money_proto =
          rule_discount_info_proto.amount_off();
      money.SetStringKey("currencyCode", money_proto.currency_code());
      money.SetStringKey("units", money_proto.units());
      money.SetIntKey("nanos", money_proto.nanos());

      discount.SetKey("amountOff", std::move(money));
    } else {
      discount.SetIntKey("percentOff", rule_discount_info_proto.percent_off());
    }
    rule_discount.SetKey("discount", std::move(discount));

    rule_discounts_list.Append(std::move(rule_discount));
  }
  discount_dict.SetKey("ruleDiscounts", std::move(rule_discounts_list));

  base::Value request_dict(base::Value::Type::DICTIONARY);
  request_dict.SetKey("discount", std::move(discount_dict));
  request_dict.SetStringKey("baseUrl", cart_content_proto.merchant_cart_url());
  request_dict.SetStringKey("merchantId", cart_discount_proto.merchant_id());

  std::string request_json;
  base::JSONWriter::Write(request_dict, &request_json);
  VLOG(2) << "Request body: " << request_json;
  return request_json;
}

void CartDiscountLinkFetcher::OnLinkFetched(
    std::unique_ptr<EndpointFetcher> endpoint_fetcher,
    CartDiscountLinkFetcherCallback callback,
    std::unique_ptr<EndpointResponse> responses) {
  VLOG(2) << "Response: " << responses->response;
  DCHECK(responses) << "responses should not be null";
  if (!responses) {
    std::move(callback).Run(GURL());
    return;
  }

  absl::optional<base::Value> value =
      base::JSONReader::Read(responses->response);

  if (!value || !value->is_dict() || !value->FindKey("url")) {
    NOTREACHED() << "empty response or wrong format";
    std::move(callback).Run(GURL());
    return;
  }
  std::move(callback).Run(GURL(value->FindKey("url")->GetString()));
}
