// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_CART_FETCH_DISCOUNT_WORKER_H_
#define CHROME_BROWSER_CART_FETCH_DISCOUNT_WORKER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/cart/cart_discount_fetcher.h"
#include "chrome/browser/cart/cart_service.h"
#include "chrome/browser/commerce/coupons/coupon_service.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"

namespace network {
class SharedURLLoaderFactory;
class PendingSharedURLLoaderFactory;
}  // namespace network

namespace signin {
class IdentityManager;
}  // namespace signin

namespace variations {
class VariationsClient;
}  // namespace variations

// Delegate class that enables FetchDiscountWorker to use discount-related
// functionalities from CartService.
class CartDiscountServiceDelegate {
 public:
  explicit CartDiscountServiceDelegate(CartService* cart_service);
  virtual ~CartDiscountServiceDelegate();
  virtual void LoadAllCarts(CartDB::LoadCallback callback);
  virtual void UpdateCart(const std::string& cart_url,
                          const cart_db::ChromeCartContentProto new_proto,
                          const bool is_tester);
  virtual void RecordFetchTimestamp();
  virtual void UpdateFreeListingCoupons(const CouponService::CouponsMap& map);

 private:
  raw_ptr<CartService, DanglingUntriaged> cart_service_;
};

// This is used to fetch discounts for active Carts in cart_db. It starts
// to work after calling Start() and continue to work util Chrome is finished.
// The flow looks as follow:
//
//   UI Thread              | backend_task_runner_
//  ===========================================
// 1) Start                 |
// 2) PrepareToFetch (delay)|
// 3) ReadyToFetch          |
// 4)                       | FetchInBackground
// 5)                       | DoneFetchingInBackground
// 6) AfterDiscountFetched  |
// 7) OnUpdatingDiscounts   |
// 8) Start                 |

// TODO(meiliang): Add an API to allow ending the work earlier. e.g. when user
// has hidden the cart module.
class FetchDiscountWorker {
 public:
  FetchDiscountWorker(
      scoped_refptr<network::SharedURLLoaderFactory>
          browserProcessURLLoaderFactory,
      std::unique_ptr<CartDiscountFetcherFactory> fetcher_factory,
      std::unique_ptr<CartDiscountServiceDelegate>
          cart_discount_service_delegate,
      signin::IdentityManager* const identity_manager,
      variations::VariationsClient* const chrome_variations_client);
  virtual ~FetchDiscountWorker();
  // Starts the worker to work.
  virtual void Start(base::TimeDelta delay);

 private:
  using AfterFetchingCallback =
      base::OnceCallback<void(CartDiscountFetcher::CartDiscountMap, bool)>;
  using ContinueToWorkCallback = base::OnceCallback<void()>;
  friend class FakeFetchDiscountWorker;

  scoped_refptr<network::SharedURLLoaderFactory>
      browserProcessURLLoaderFactory_;
  // The TaskRunner to which the getting discounts background tasks are posted.
  scoped_refptr<base::SequencedTaskRunner> backend_task_runner_;
  // This is used to create a CartDiscountFetcher to fetch discounts.
  std::unique_ptr<CartDiscountFetcherFactory> fetcher_factory_;
  // This is used to access CartService discount-related functionalities such as
  // loading all active carts, updating given cart discount, etc.
  std::unique_ptr<CartDiscountServiceDelegate> cart_discount_service_delegate_;
  // This is used to identify whether user is a sync user.
  const raw_ptr<signin::IdentityManager, DanglingUntriaged> identity_manager_;
  // This is used to fetch the oauth token.
  std::unique_ptr<const signin::PrimaryAccountAccessTokenFetcher>
      access_token_fetcher_;
  const raw_ptr<variations::VariationsClient, DanglingUntriaged>
      chrome_variations_client_;

  // This is run in the UI thread, it loads all active carts.
  void PrepareToFetch();

  // This is run if user is a sync user.
  void FetchOauthToken();

  // This is run after oauth token is fetched.
  void OnAuthTokenFetched(GoogleServiceAuthError error,
                          signin::AccessTokenInfo access_token_info);

  // Load all the active carts.
  void LoadAllActiveCarts(bool is_oauth_fetch, std::string access_token_str);

  // This is run in the UI thread, it posts the discount fetching work,
  // FetchInBackground(), to another thread as a background task.
  void ReadyToFetch(bool is_oauth_fetch,
                    std::string access_token_str,
                    bool success,
                    std::vector<CartDB::KeyAndValue> proto_pairs);

  std::string GetVariationsHeaders();

  // TODO(crbug.com/40181210): Change these two static method to anonymous
  // namespace in the cc file. This is run in a background thread, it fetches
  // for discounts for all active carts.
  static void FetchInBackground(
      std::unique_ptr<network::PendingSharedURLLoaderFactory> pending_factory,
      std::unique_ptr<CartDiscountFetcher> fetcher,
      AfterFetchingCallback after_fetching_callback,
      std::vector<CartDB::KeyAndValue> proto_pairs,
      bool is_oauth_fetch,
      std::string access_token_str,
      std::string fetch_for_locale,
      std::string variation_headers);

  // This is run in a background thread, it posts AfterDiscountFetched() back to
  // UI thread to process the fetched result.
  static void DoneFetchingInBackground(
      AfterFetchingCallback after_fetching_callback,
      CartDiscountFetcher::CartDiscountMap discounts,
      bool is_tester);

  // This is run in the UI thread, it loads all active carts to update its
  // discount.
  void AfterDiscountFetched(CartDiscountFetcher::CartDiscountMap discounts,
                            bool is_tester);

  // This is run in the UI thread, it updates discounts for all the active
  // carts. It also post PrepareToFetch() to continue fetching in the
  // background.
  void OnUpdatingDiscounts(CartDiscountFetcher::CartDiscountMap discounts,
                           bool is_tester,
                           bool success,
                           std::vector<CartDB::KeyAndValue> proto_pairs);

  base::WeakPtrFactory<FetchDiscountWorker> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_CART_FETCH_DISCOUNT_WORKER_H_
