// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_CART_CART_DISCOUNT_FETCHER_H_
#define CHROME_BROWSER_CART_CART_DISCOUNT_FETCHER_H_

#include <memory>
#include <unordered_map>

#include "base/memory/scoped_refptr.h"
#include "chrome/browser/cart/cart_db.h"
#include "chrome/browser/cart/cart_db_content.pb.h"
#include "chrome/browser/endpoint_fetcher/endpoint_fetcher.h"

namespace network {
class PendingSharedURLLoaderFactory;
}  // namespace network

class CartDiscountFetcher {
 public:
  // base::flat_map is used here for optimization, since the number of carts are
  // expected to be low (< 100) at this stage. Need to use std::map when number
  // gets larger.
  using CartDiscountMap =
      base::flat_map<std::string, std::vector<cart_db::DiscountInfoProto>>;

  using CartDiscountFetcherCallback = base::OnceCallback<void(CartDiscountMap)>;

  virtual ~CartDiscountFetcher();

  virtual void Fetch(
      std::unique_ptr<network::PendingSharedURLLoaderFactory> pending_factory,
      CartDiscountFetcherCallback callback);

 private:
  // TODO(meiliang): Add param a list of carts to fetch.
  static void fetchForDiscounts(
      std::unique_ptr<network::PendingSharedURLLoaderFactory> pending_factory,
      CartDiscountFetcherCallback callback);
  static void OnDiscountsAvailable(
      std::unique_ptr<EndpointFetcher> endpoint_fetcher,
      CartDiscountFetcherCallback callback,
      std::unique_ptr<EndpointResponse> responses);
  static std::unique_ptr<EndpointFetcher> CreateEndpointFetcher(
      std::unique_ptr<network::PendingSharedURLLoaderFactory> pending_factory);
};

class CartDiscountFetcherFactory {
 public:
  virtual ~CartDiscountFetcherFactory();
  virtual std::unique_ptr<CartDiscountFetcher> createFetcher();
};

#endif  // CHROME_BROWSER_CART_CART_DISCOUNT_FETCHER_H_
