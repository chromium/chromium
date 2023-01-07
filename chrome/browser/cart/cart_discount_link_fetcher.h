// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_CART_CART_DISCOUNT_LINK_FETCHER_H_
#define CHROME_BROWSER_CART_CART_DISCOUNT_LINK_FETCHER_H_

#include "base/callback.h"
#include "components/commerce/core/proto/cart_db_content.pb.h"
#include "components/endpoint_fetcher/endpoint_fetcher.h"

namespace network {
class PendingSharedURLLoaderFactory;
}  // namespace network

// This is used to get an encrypted discount link.
class CartDiscountLinkFetcher {
 public:
  using CartDiscountLinkFetcherCallback = base::OnceCallback<void(const GURL&)>;

  virtual ~CartDiscountLinkFetcher();

  // Fetches the encrypted link for the given |cart_content_proto|.
  virtual void Fetch(
      std::unique_ptr<network::PendingSharedURLLoaderFactory> pending_factory,
      cart_db::ChromeCartContentProto cart_content_proto,
      CartDiscountLinkFetcherCallback callback);

 private:
  friend class CartDiscountLinkFetcherTest;
  // TODO(crbug.com/1207197): Move these static method to the anonymous
  // namespace in the cc file.
  static std::unique_ptr<EndpointFetcher> CreateEndpointFetcher(
      std::unique_ptr<network::PendingSharedURLLoaderFactory> pending_factory,
      cart_db::ChromeCartContentProto cart_content_proto);
  static std::string GeneratePostData(
      cart_db::ChromeCartContentProto cart_content_proto);
  static void OnLinkFetched(std::unique_ptr<EndpointFetcher> endpoint_fetcher,
                            CartDiscountLinkFetcherCallback callback,
                            std::unique_ptr<EndpointResponse> responses);
};

#endif  // CHROME_BROWSER_CART_CART_DISCOUNT_LINK_FETCHER_H_
