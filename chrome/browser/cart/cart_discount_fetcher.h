// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_CART_CART_DISCOUNT_FETCHER_H_
#define CHROME_BROWSER_CART_CART_DISCOUNT_FETCHER_H_

#include <memory>
#include <unordered_map>

#include "base/containers/flat_map.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/cart/cart_db.h"
#include "components/commerce/core/proto/cart_db_content.pb.h"
#include "components/commerce/core/proto/coupon_db_content.pb.h"
#include "components/endpoint_fetcher/endpoint_fetcher.h"

namespace network {
class PendingSharedURLLoaderFactory;
}  // namespace network

struct MerchantIdAndDiscounts {
 public:
  std::string merchant_id;
  std::vector<cart_db::RuleDiscountInfoProto> rule_discounts;
  std::vector<coupon_db::FreeListingCouponInfoProto> coupon_discounts;
  std::string highest_discount_string;
  bool has_coupons;

  explicit MerchantIdAndDiscounts(
      std::string merchant_id,
      std::vector<cart_db::RuleDiscountInfoProto> rule_discounts,
      std::vector<coupon_db::FreeListingCouponInfoProto> coupon_discounts,
      std::string discount_string,
      bool has_coupons);
  MerchantIdAndDiscounts(const MerchantIdAndDiscounts& other);
  MerchantIdAndDiscounts& operator=(const MerchantIdAndDiscounts& other);
  MerchantIdAndDiscounts(MerchantIdAndDiscounts&& other);
  MerchantIdAndDiscounts& operator=(MerchantIdAndDiscounts&& other);
  ~MerchantIdAndDiscounts();
};

class CartDiscountFetcher {
 public:
  // base::flat_map is used here for optimization, since the number of carts are
  // expected to be low (< 100) at this stage. Need to use std::map when number
  // gets larger.
  using CartDiscountMap = base::flat_map<std::string, MerchantIdAndDiscounts>;

  using CartDiscountFetcherCallback =
      base::OnceCallback<void(CartDiscountMap, bool)>;

  virtual ~CartDiscountFetcher();

  virtual void Fetch(
      std::unique_ptr<network::PendingSharedURLLoaderFactory> pending_factory,
      CartDiscountFetcherCallback callback,
      std::vector<CartDB::KeyAndValue> proto_pairs,
      bool is_oauth_fetch,
      std::string access_token,
      std::string fetch_for_locale,
      std::string variation_headers);

 private:
  friend class CartDiscountFetcherTest;
  static void FetchForDiscounts(
      std::unique_ptr<network::PendingSharedURLLoaderFactory> pending_factory,
      CartDiscountFetcherCallback callback,
      std::vector<CartDB::KeyAndValue> proto_pairs,
      bool is_oauth_fetch,
      std::string access_token,
      std::string fetch_for_locale,
      std::string variation_headers);
  static void OnDiscountsAvailable(
      std::unique_ptr<EndpointFetcher> endpoint_fetcher,
      CartDiscountFetcherCallback callback,
      std::unique_ptr<EndpointResponse> responses);
  static std::unique_ptr<EndpointFetcher> CreateEndpointFetcher(
      std::unique_ptr<network::PendingSharedURLLoaderFactory> pending_factory,
      std::vector<CartDB::KeyAndValue> proto_pairs,
      bool is_oauth_fetch,
      std::string fetch_for_locale,
      std::string variation_headers);
  static std::string generatePostData(
      const std::vector<CartDB::KeyAndValue>& proto_pairs,
      base::Time current_timestamp);
};

class CartDiscountFetcherFactory {
 public:
  virtual ~CartDiscountFetcherFactory();
  virtual std::unique_ptr<CartDiscountFetcher> createFetcher();
};

#endif  // CHROME_BROWSER_CART_CART_DISCOUNT_FETCHER_H_
