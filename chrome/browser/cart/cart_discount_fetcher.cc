// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cart/cart_discount_fetcher.h"

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/cart/cart_db.h"
#include "chrome/browser/cart/cart_discount_metric_collector.h"
#include "chrome/grit/generated_resources.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/proto/cart_db_content.pb.h"
#include "components/commerce/core/proto/coupon_db_content.pb.h"
#include "components/endpoint_fetcher/endpoint_fetcher.h"
#include "components/search/ntp_features.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/cross_thread_pending_shared_url_loader_factory.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "ui/base/l10n/l10n_util.h"

namespace {
const char kPostMethod[] = "POST";
const char kContentType[] = "application/json; charset=UTF-8";
const char kAcceptLanguageKey[] = "Accept-Language";
// The name string for the header for variations information.
const char kClientDataHeader[] = "X-Client-Data";

const char kFetchDiscountsEndpoint[] =
    "https://memex-pa.googleapis.com/v1/shopping/cart/discounts";
constexpr base::TimeDelta kTimeout = base::Milliseconds(30000);

const char kCartDiscountFetcherEndpointParam[] =
    "CartDiscountFetcherEndpointParam";

constexpr base::FeatureParam<std::string> kDiscountFetcherServerConfigEndpoint{
    &ntp_features::kNtpChromeCartModule, kCartDiscountFetcherEndpointParam,
    kFetchDiscountsEndpoint};

struct RuleDiscountInfo {
  std::vector<cart_db::RuleDiscountInfoProto> discount_list;
  int highest_amount_off;
  int highest_percent_off;

  RuleDiscountInfo(std::vector<cart_db::RuleDiscountInfoProto> discount_list,
                   int highest_amount_off,
                   int highest_percent_off)
      : discount_list(std::move(discount_list)),
        highest_amount_off(highest_amount_off),
        highest_percent_off(highest_percent_off) {}

  ~RuleDiscountInfo() = default;
  RuleDiscountInfo(const RuleDiscountInfo& other) = delete;
  RuleDiscountInfo& operator=(const RuleDiscountInfo& other) = delete;
  RuleDiscountInfo(RuleDiscountInfo&& other) = default;
  RuleDiscountInfo& operator=(RuleDiscountInfo&& other) = default;
};

struct CouponDiscountInfo {
  std::vector<coupon_db::FreeListingCouponInfoProto> discount_list;
  explicit CouponDiscountInfo(
      std::vector<coupon_db::FreeListingCouponInfoProto> discount_list)
      : discount_list(std::move(discount_list)) {}

  ~CouponDiscountInfo() = default;
  CouponDiscountInfo(const CouponDiscountInfo& other) = delete;
  CouponDiscountInfo& operator=(const CouponDiscountInfo& other) = delete;
  CouponDiscountInfo(CouponDiscountInfo&& other) = default;
  CouponDiscountInfo& operator=(CouponDiscountInfo&& other) = default;
};

enum CouponType {
  UNSPECIFIED,
  FREE_LISTING_WITHOUT_CODE,
  FREE_LISTING_WITH_CODE,
  RBD_WITH_CODE
};

// TODO(crbug.com/40181210): Consolidate to one util method to get string.
std::string GetMerchantUrl(const base::Value::Dict* merchant_identifier) {
  // TODO(crbug.com/40181210): Use a static constant for "cartUrl" instead.
  const std::string* value = merchant_identifier->FindString("cartUrl");
  if (!value) {
    LOG(WARNING) << "Missing cart_url or it is not a string";
    return "";
  }

  return *value;
}

std::string GetMerchantId(const base::Value::Dict* merchant_identifier) {
  const std::string* value = merchant_identifier->FindString("merchantId");
  if (!value) {
    LOG(WARNING) << "Missing merchant_id or it is not a string";
    return "";
  }

  return *value;
}

std::string GetStringFromDict(const base::Value* dict,
                              std::string_view key,
                              bool is_required) {
  DCHECK(dict->is_dict());

  const std::string* value = dict->GetDict().FindString(key);
  if (!value) {
    if (is_required) {
      LOG(WARNING) << "Missing " << key << " or it is not a string";
    }
    return "";
  }

  return *value;
}

RuleDiscountInfo CovertToRuleDiscountInfo(
    const base::Value* rule_discount_list) {
  std::vector<cart_db::RuleDiscountInfoProto> cart_discounts;

  if (!rule_discount_list || !rule_discount_list->is_list()) {
    return RuleDiscountInfo(cart_discounts, 0 /*highest_amount_off*/,
                            0 /*highest_percent_off*/);
  }

  cart_discounts.reserve(rule_discount_list->GetList().size());

  int highest_percent_off = 0;
  int64_t highest_amount_off = 0;
  for (const auto& rule_discount : rule_discount_list->GetList()) {
    cart_db::RuleDiscountInfoProto discount_proto;
    const base::Value::Dict& rule_discount_dict = rule_discount.GetDict();

    // Parse ruleId
    const std::string* rule_id = rule_discount_dict.FindString("ruleId");
    if (!rule_id) {
      LOG(WARNING) << "Missing rule_id or it is not a string";
      continue;
    }
    discount_proto.set_rule_id(*rule_id);

    // Parse merchantRuleId
    const std::string* merchant_rule_id =
        rule_discount_dict.FindString("merchantRuleId");
    if (!merchant_rule_id) {
      LOG(WARNING) << "Missing merchant_rule_id or it is not a string";
      continue;
    }
    discount_proto.set_merchant_rule_id(*merchant_rule_id);

    // Parse rawMerchantOfferId
    const base::Value* raw_merchant_offer_id_value =
        rule_discount_dict.Find("rawMerchantOfferId");
    if (!raw_merchant_offer_id_value) {
      VLOG(1) << "raw_merchant_offer_id is empty";
    } else if (!raw_merchant_offer_id_value->is_string()) {
      LOG(WARNING) << "raw_merchant_offer_id is not a string";
      continue;
    } else {
      discount_proto.set_raw_merchant_offer_id(
          raw_merchant_offer_id_value->GetString());
    }

    // Parse discount
    const base::Value::Dict* discount_dict =
        rule_discount_dict.FindDict("discount");
    if (!discount_dict) {
      LOG(WARNING) << "discount is missing or it is not a dictionary";
      continue;
    }

    if (discount_dict->Find("percentOff")) {
      std::optional<int> percent_off = discount_dict->FindInt("percentOff");
      if (!percent_off.has_value()) {
        LOG(WARNING) << "percent_off is not a int";
        continue;
      }
      discount_proto.set_percent_off(*percent_off);
      highest_percent_off = std::max(highest_percent_off, *percent_off);
    } else {
      const base::Value::Dict* amount_off_dict =
          discount_dict->FindDict("amountOff");
      if (!amount_off_dict) {
        LOG(WARNING) << "amount_off is not a dictionary";
        continue;
      }

      auto* money = discount_proto.mutable_amount_off();
      // Parse currencyCode
      const std::string* currency_code_value =
          amount_off_dict->FindString("currencyCode");
      if (!currency_code_value) {
        LOG(WARNING) << "Missing currency_code or it is not a string";
        continue;
      }
      money->set_currency_code(*currency_code_value);

      // Parse units
      const base::Value* units_value = amount_off_dict->Find("units");
      if (!units_value || !units_value->is_string()) {
        LOG(WARNING) << "Missing units or it is not a string, it is a "
                     << units_value->type();
        continue;
      }
      std::string units_string = units_value->GetString();
      money->set_units(units_string);
      int64_t units;
      base::StringToInt64(units_string, &units);
      highest_amount_off = std::max(highest_amount_off, units);

      // Parse nanos
      std::optional<int> nano = amount_off_dict->FindInt("nanos");
      if (!nano.has_value()) {
        LOG(WARNING) << "Missing nanos or it is not a int";
        continue;
      }
      money->set_nanos(*nano);
    }

    cart_discounts.emplace_back(std::move(discount_proto));
  }

  return RuleDiscountInfo(std::move(cart_discounts), highest_amount_off,
                          highest_percent_off);
}

CouponType ConvertToCouponType(const base::Value* type) {
  if (!type || !type->is_string()) {
    LOG(WARNING) << "Missing coupon type";
    return CouponType::UNSPECIFIED;
  }

  std::string type_str = type->GetString();
  if (type_str == "FREE_LISTING_WITHOUT_CODE") {
    return CouponType::FREE_LISTING_WITHOUT_CODE;
  } else if (type_str == "FREE_LISTING_WITH_CODE") {
    return CouponType::FREE_LISTING_WITH_CODE;
  } else if (type_str == "RBD_WITH_CODE") {
    return CouponType::RBD_WITH_CODE;
  }

  LOG(WARNING) << "Unrecognized coupon type";
  return CouponType::UNSPECIFIED;
}

bool IsCouponWithCode(CouponType type) {
  if (type == CouponType::FREE_LISTING_WITH_CODE)
    return true;
  if (commerce::kCodeBasedRuleDiscount.Get() &&
      type == CouponType::RBD_WITH_CODE) {
    return true;
  }
  return false;
}

CouponDiscountInfo ConvertToCouponDiscountInfo(
    const base::Value* coupon_discount_list) {
  std::vector<coupon_db::FreeListingCouponInfoProto> coupons;
  if (!commerce::IsCouponWithCodeEnabled() || !coupon_discount_list ||
      !coupon_discount_list->is_list()) {
    return CouponDiscountInfo({});
  }

  coupons.reserve(coupon_discount_list->GetList().size());

  for (const auto& coupon_discount : coupon_discount_list->GetList()) {
    const base::Value::Dict& coupon_discount_dict = coupon_discount.GetDict();
    coupon_db::FreeListingCouponInfoProto coupon_info_proto;

    // Parse type
    CouponType type = ConvertToCouponType(coupon_discount_dict.Find("type"));
    if (!IsCouponWithCode(type))
      continue;

    // Parse description
    // TODO(crbug.com/40801865): Need to parse languageCode and save it in
    // coupon_info_proto.
    coupon_info_proto.set_coupon_description(GetStringFromDict(
        coupon_discount_dict.Find("description"), "title", true));

    // Parse couponCode
    coupon_info_proto.set_coupon_code(
        GetStringFromDict(&coupon_discount, "couponCode", true));

    // Parse couponId
    int64_t coupon_id;
    if (!base::StringToInt64(
            GetStringFromDict(&coupon_discount, "couponId", true),
            &coupon_id)) {
      LOG(WARNING) << "Failed to parsed couponId";
      continue;
    }
    coupon_info_proto.set_coupon_id(coupon_id);

    // Parse expiryTimeSec
    const base::Value* expiry_time_sec_value =
        coupon_discount_dict.Find("expiryTimeSec");
    if (!expiry_time_sec_value) {
      LOG(WARNING) << "Missing expiryTimeSec";
      continue;
    }
    if (expiry_time_sec_value->GetIfDouble() ||
        expiry_time_sec_value->GetIfInt()) {
      coupon_info_proto.set_expiry_time(expiry_time_sec_value->GetDouble());
    } else {
      LOG(WARNING) << "expiryTimeSec is in a wrong format: "
                   << expiry_time_sec_value->type();
      continue;
    }

    coupons.emplace_back(std::move(coupon_info_proto));
  }

  return CouponDiscountInfo(std::move(coupons));
}

bool ValidateResponse(const std::optional<base::Value>& response) {
  if (!response) {
    LOG(WARNING) << "Response is not valid";
    return false;
  }

  if (!response->is_dict()) {
    LOG(WARNING)
        << "Wrong response format, response is not a dictionary. Response: "
        << response->DebugString();
    return false;
  }

  if (response->GetDict().empty()) {
    VLOG(1) << "Response does not have value. Response: "
            << response->DebugString();
    return false;
  }
  return true;
}
}  // namespace

MerchantIdAndDiscounts::MerchantIdAndDiscounts(
    std::string merchant_id,
    std::vector<cart_db::RuleDiscountInfoProto> rule_discounts,
    std::vector<coupon_db::FreeListingCouponInfoProto> coupon_discounts,
    std::string discount_string,
    bool has_coupons)
    : merchant_id(std::move(merchant_id)),
      rule_discounts(std::move(rule_discounts)),
      coupon_discounts(std::move(coupon_discounts)),
      highest_discount_string(std::move(discount_string)),
      has_coupons(has_coupons) {}

MerchantIdAndDiscounts::MerchantIdAndDiscounts(
    const MerchantIdAndDiscounts& other) = default;

MerchantIdAndDiscounts& MerchantIdAndDiscounts::operator=(
    const MerchantIdAndDiscounts& other) = default;

MerchantIdAndDiscounts::MerchantIdAndDiscounts(MerchantIdAndDiscounts&& other) =
    default;

MerchantIdAndDiscounts& MerchantIdAndDiscounts::operator=(
    MerchantIdAndDiscounts&& other) = default;

MerchantIdAndDiscounts::~MerchantIdAndDiscounts() = default;

std::unique_ptr<CartDiscountFetcher>
CartDiscountFetcherFactory::createFetcher() {
  return std::make_unique<CartDiscountFetcher>();
}

CartDiscountFetcherFactory::~CartDiscountFetcherFactory() = default;

CartDiscountFetcher::~CartDiscountFetcher() = default;

void CartDiscountFetcher::Fetch(
    std::unique_ptr<network::PendingSharedURLLoaderFactory> pending_factory,
    CartDiscountFetcherCallback callback,
    std::vector<CartDB::KeyAndValue> proto_pairs,
    bool is_oauth_fetch,
    std::string access_token,
    std::string fetch_for_locale,
    std::string variation_headers) {
  CartDiscountFetcher::FetchForDiscounts(
      std::move(pending_factory), std::move(callback), std::move(proto_pairs),
      is_oauth_fetch, std::move(access_token), std::move(fetch_for_locale),
      std::move(variation_headers));
}

void CartDiscountFetcher::FetchForDiscounts(
    std::unique_ptr<network::PendingSharedURLLoaderFactory> pending_factory,
    CartDiscountFetcherCallback callback,
    std::vector<CartDB::KeyAndValue> proto_pairs,
    bool is_oauth_fetch,
    std::string access_token,
    std::string fetch_for_locale,
    std::string variation_headers) {
  auto fetcher = CreateEndpointFetcher(
      std::move(pending_factory), std::move(proto_pairs), is_oauth_fetch,
      std::move(fetch_for_locale), std::move(variation_headers));

  auto* const fetcher_ptr = fetcher.get();
  fetcher_ptr->PerformRequest(
      base::BindOnce(&CartDiscountFetcher::OnDiscountsAvailable,
                     std::move(fetcher), std::move(callback)),
      access_token.c_str());
  CartDiscountMetricCollector::RecordFetchingForDiscounts();
}

std::unique_ptr<EndpointFetcher> CartDiscountFetcher::CreateEndpointFetcher(
    std::unique_ptr<network::PendingSharedURLLoaderFactory> pending_factory,
    std::vector<CartDB::KeyAndValue> proto_pairs,
    bool is_oauth_fetch,
    std::string fetch_for_locale,
    std::string variation_headers) {
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("chrome_cart_discounts_lookup", R"(
        semantics {
          sender: "Chrome Cart"
          description:
            "Chrome looks up any discounts available to users' Chrome Shopping "
            "Carts. The Chrome Shopping Cart list is displayed on the New Tab "
            "Page, and it contains users' pending shopping Carts from merchant "
            "sites. Currently, this is a device based feature, Google does "
            "not save any data that is sent."
          trigger:
            "After user has given their consent and opt-in for the feature."
            "Afterwards, refreshes every 30 minutes."
          data:
            "The Chrome Cart data, includes the shopping site and products "
            "users have added to their shopping carts."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "You can enable or disable this feature via the Chrome NTP "
            "customized page in the bottom right corner of the NTP."
          policy_exception_justification: "No policy provided because this "
            "does not require user to sign in or sync, and they must given "
            "their consent before triggering this. And user can disable this "
            "feature."
        })");

  const std::vector<std::string> headers{kAcceptLanguageKey, std::move(fetch_for_locale)};
  const std::vector<std::string>& cors_exempt_headers{
      kClientDataHeader, std::move(variation_headers)};

  return std::make_unique<EndpointFetcher>(
      GURL(kDiscountFetcherServerConfigEndpoint.Get()), kPostMethod,
      kContentType, kTimeout, generatePostData(proto_pairs, base::Time::Now()),
      headers, cors_exempt_headers, traffic_annotation,
      network::SharedURLLoaderFactory::Create(std::move(pending_factory)),
      is_oauth_fetch);
}

std::string CartDiscountFetcher::generatePostData(
    const std::vector<CartDB::KeyAndValue>& proto_pairs,
    base::Time current_time) {
  base::Value::List carts_list;

  for (const CartDB::KeyAndValue& key_and_value : proto_pairs) {
    cart_db::ChromeCartContentProto cart_proto = key_and_value.second;

    base::Value::Dict cart_dict;
    // Set merchantIdentifier.
    base::Value* merchant_dict =
        cart_dict.Set("merchantIdentifier", base::Value::Dict());
    merchant_dict->GetDict().Set("cartUrl", cart_proto.merchant_cart_url());

    // Set CartAbandonedTimeMinutes.
    int cart_abandoned_time_mintues =
        (current_time -
         base::Time::FromSecondsSinceUnixEpoch(cart_proto.timestamp()))
            .InMinutes();
    cart_dict.Set("cartAbandonedTimeMinutes", cart_abandoned_time_mintues);

    // Set rawMerchantOffers.
    base::Value::List offer_list;
    for (const auto& product_proto : cart_proto.product_infos()) {
      offer_list.Append(product_proto.product_id());
    }
    cart_dict.Set("rawMerchantOffers", std::move(offer_list));

    // Add cart_dict to cart_list.
    carts_list.Append(std::move(cart_dict));
  }

  base::Value::Dict request_dic;
  request_dic.Set("carts", std::move(carts_list));

  std::string request_json;
  base::JSONWriter::Write(request_dic, &request_json);
  VLOG(2) << "Request body: " << request_json;
  return request_json;
}

void CartDiscountFetcher::OnDiscountsAvailable(
    std::unique_ptr<EndpointFetcher> endpoint_fetcher,
    CartDiscountFetcherCallback callback,
    std::unique_ptr<EndpointResponse> responses) {
  VLOG(2) << "Response: " << responses->response;
  CartDiscountMap cart_discount_map;
  std::optional<base::Value> value =
      base::JSONReader::Read(responses->response);
  if (!ValidateResponse(value)) {
    std::move(callback).Run(std::move(cart_discount_map), false);
    return;
  }

  base::Value::Dict& dict = value->GetDict();
  const base::Value* error_value = dict.Find("error");
  if (error_value) {
    LOG(WARNING) << "Error: " << responses->response;
    std::move(callback).Run(std::move(cart_discount_map), false);
    return;
  }

  const base::Value::List* discounts_list = dict.FindList("discounts");
  if (!discounts_list) {
    LOG(WARNING) << "Missing discounts or it is not a list";
    std::move(callback).Run(std::move(cart_discount_map), false);
    return;
  }

  for (const auto& merchant_discount : *discounts_list) {
    // Parse merchant_identifier.
    const base::Value::Dict& merchant_discount_dict =
        merchant_discount.GetDict();
    const base::Value::Dict* merchant_identifier =
        merchant_discount_dict.FindDict("merchantIdentifier");
    if (!merchant_identifier) {
      LOG(WARNING) << "Missing merchant_identifier";
      continue;
    }
    std::string merchant_url = GetMerchantUrl(merchant_identifier);
    if (merchant_url.empty()) {
      continue;
    }

    std::string merchant_id = GetMerchantId(merchant_identifier);
    if (merchant_id.empty()) {
      continue;
    }

    std::string discount_string = "";

    // Parse overallDiscountInfo, which is an optional field.
    const base::Value* overall_discount_info =
        merchant_discount_dict.Find("overallDiscountInfo");
    if (overall_discount_info) {
      discount_string = GetStringFromDict(overall_discount_info, "text",
                                          true /*is_required*/);
    }

    // Parse rule discounts, which is an optional field.
    auto cart_rule_based_discounts_info =
        CovertToRuleDiscountInfo(merchant_discount_dict.Find("ruleDiscounts"));

    if (cart_rule_based_discounts_info.discount_list.size() > 0) {
      std::string discount_string_param;
      if (cart_rule_based_discounts_info.highest_amount_off) {
        // TODO(meiliang): Use icu_formatter or
        // components/payments/core/currency_formatter to set the amount off.
        discount_string_param =
            "$" + base::NumberToString(
                      cart_rule_based_discounts_info.highest_amount_off);
      } else if (cart_rule_based_discounts_info.highest_percent_off) {
        discount_string_param =
            base::NumberToString(
                cart_rule_based_discounts_info.highest_percent_off) +
            "%";
      } else {
        LOG(WARNING) << "Missing hightest discount info";
        continue;
      }

      if (discount_string.empty()) {
        discount_string =
            cart_rule_based_discounts_info.discount_list.size() > 1
                ? l10n_util::GetStringFUTF8(
                      IDS_NTP_MODULES_CART_DISCOUNT_CHIP_UP_TO_AMOUNT,
                      base::UTF8ToUTF16(discount_string_param))
                : l10n_util::GetStringFUTF8(
                      IDS_NTP_MODULES_CART_DISCOUNT_CHIP_AMOUNT,
                      base::UTF8ToUTF16(discount_string_param));
      }
    }

    // Parse couponDiscounts, which is an optional field.
    const base::Value* coupon_discounts =
        merchant_discount_dict.Find("couponDiscounts");

    auto coupon_discount_info = ConvertToCouponDiscountInfo(coupon_discounts);

    MerchantIdAndDiscounts merchant_id_and_discounts(
        std::move(merchant_id),
        std::move(cart_rule_based_discounts_info.discount_list),
        std::move(coupon_discount_info.discount_list),
        std::move(discount_string), coupon_discounts != nullptr);
    cart_discount_map.emplace(merchant_url,
                              std::move(merchant_id_and_discounts));
  }

  bool is_tester = false;
  std::optional<bool> is_tester_value = dict.FindBool("externalTester");
  if (is_tester_value.has_value()) {
    is_tester = *is_tester_value;
  } else {
    std::optional<bool> is_internal_tester_value =
        dict.FindBool("internalTester");
    if (is_internal_tester_value.has_value()) {
      is_tester = *is_internal_tester_value;
    }
  }

  std::move(callback).Run(std::move(cart_discount_map), is_tester);
}
