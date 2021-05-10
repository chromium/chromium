// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cart/cart_discount_fetcher.h"

#include "base/json/json_writer.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "chrome/browser/endpoint_fetcher/endpoint_fetcher.h"
#include "chrome/browser/profiles/profile.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/cross_thread_pending_shared_url_loader_factory.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace {
const char POST_METHOD[] = "POST";
const char CONTENT_TYPE[] = "application/json; charset=UTF-8";

const char FETCH_DISCOUNTS_ENDPOINT[] =
    "https://memex-pa.googleapis.com/v1/shopping/cart/discounts";
const int64_t TIMEOUT_MS = 1000;
}  // namespace

std::unique_ptr<CartDiscountFetcher>
CartDiscountFetcherFactory::createFetcher() {
  return std::make_unique<CartDiscountFetcher>();
}

CartDiscountFetcherFactory::~CartDiscountFetcherFactory() = default;

CartDiscountFetcher::~CartDiscountFetcher() = default;

void CartDiscountFetcher::Fetch(
    std::unique_ptr<network::PendingSharedURLLoaderFactory> pending_factory,
    CartDiscountFetcherCallback callback) {
  CartDiscountFetcher::fetchForDiscounts(std::move(pending_factory),
                                         std::move(callback));
}

void CartDiscountFetcher::fetchForDiscounts(
    std::unique_ptr<network::PendingSharedURLLoaderFactory> pending_factory,
    CartDiscountFetcherCallback callback) {
  auto fetcher = CreateEndpointFetcher(std::move(pending_factory));

  auto* const fetcher_ptr = fetcher.get();
  fetcher_ptr->PerformRequest(
      base::BindOnce(&CartDiscountFetcher::OnDiscountsAvailable,
                     std::move(fetcher), std::move(callback)),
      nullptr);
}

std::unique_ptr<EndpointFetcher> CartDiscountFetcher::CreateEndpointFetcher(
    std::unique_ptr<network::PendingSharedURLLoaderFactory> pending_factory) {
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

  std::string post_data = "";
  return std::make_unique<EndpointFetcher>(
      GURL(FETCH_DISCOUNTS_ENDPOINT), POST_METHOD, CONTENT_TYPE, TIMEOUT_MS,
      post_data, traffic_annotation,
      network::SharedURLLoaderFactory::Create(std::move(pending_factory)));
}

void CartDiscountFetcher::OnDiscountsAvailable(
    std::unique_ptr<EndpointFetcher> endpoint_fetcher,
    CartDiscountFetcherCallback callback,
    std::unique_ptr<EndpointResponse> responses) {
  // TODO(meiliang): parse response;
  CartDiscountMap result;
  std::move(callback).Run(std::move(result));
}
