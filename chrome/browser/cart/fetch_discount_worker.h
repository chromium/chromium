// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_CART_FETCH_DISCOUNT_WORKER_H_
#define CHROME_BROWSER_CART_FETCH_DISCOUNT_WORKER_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/cart/cart_discount_fetcher.h"
#include "chrome/browser/cart/cart_service.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"

namespace network {
class SharedURLLoaderFactory;
class PendingSharedURLLoaderFactory;
}  // namespace network

namespace signin {
class IdentityManager;
}  // namespace signin

class CartLoader {
 public:
  explicit CartLoader(Profile* profile);
  virtual ~CartLoader();
  virtual void LoadAllCarts(CartDB::LoadCallback callback);

 private:
  CartService* cart_service_;
};

class CartDiscountUpdater {
 public:
  explicit CartDiscountUpdater(Profile* profile);
  virtual ~CartDiscountUpdater();
  virtual void update(const std::string& cart_url,
                      const cart_db::ChromeCartContentProto new_proto,
                      const bool is_tester);

 private:
  CartService* cart_service_;
};

class CartLoaderAndUpdaterFactory {
 public:
  // TODO(crbug.com/1207197): Investigate to pass in Cartservice directly.
  explicit CartLoaderAndUpdaterFactory(Profile* profile);
  virtual ~CartLoaderAndUpdaterFactory();
  virtual std::unique_ptr<CartLoader> createCartLoader();
  virtual std::unique_ptr<CartDiscountUpdater> createCartDiscountUpdater();

 private:
  Profile* profile_;
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
      std::unique_ptr<CartLoaderAndUpdaterFactory> cartLoaderAndUpdaterFactory,
      signin::IdentityManager* const identity_manager);
  ~FetchDiscountWorker();
  // Starts the worker to work.
  void Start(base::TimeDelta delay);

 private:
  using AfterFetchingCallback =
      base::OnceCallback<void(CartDiscountFetcher::CartDiscountMap, bool)>;
  using ContinueToWorkCallback = base::OnceCallback<void()>;

  scoped_refptr<network::SharedURLLoaderFactory>
      browserProcessURLLoaderFactory_;
  // The TaskRunner to which the getting discounts background tasks are posted.
  scoped_refptr<base::SequencedTaskRunner> backend_task_runner_;
  // This is used to create a CartDiscountFetcher to fetch discounts.
  std::unique_ptr<CartDiscountFetcherFactory> fetcher_factory_;
  // This is used to create CartLoader to load all active carts, and
  // CartDiscountUpdater to update the given cart discount.
  std::unique_ptr<CartLoaderAndUpdaterFactory> cart_loader_and_updater_factory_;
  // This is used to identify whether user is a sync user.
  signin::IdentityManager* const identity_manager_;
  // This is used to fetch the oauth token.
  std::unique_ptr<const signin::PrimaryAccountAccessTokenFetcher>
      access_token_fetcher_;

  // This is run in the UI thread, it creates a `CartLoader` and loads all
  // active carts.
  void PrepareToFetch();

  // This is run if user is a sync user.
  void FetchOauthToken();

  // This is run after oauth token is fetched.
  void OnAuthTokenFetched(GoogleServiceAuthError error,
                          signin::AccessTokenInfo access_token_info);

  // Load all the active carts.
  void LoadAllActiveCarts(const bool is_oauth_fetch,
                          const std::string access_token_str);

  // This is run in the UI thread, it posts the discount fetching work,
  // FetchInBackground(), to another thread as a background task.
  void ReadyToFetch(const bool is_oauth_fetch,
                    const std::string access_token_str,
                    bool success,
                    std::vector<CartDB::KeyAndValue> proto_pairs);

  // TODO(crbug.com/1207197): Change these two static method to anonymous
  // namespace in the cc file. This is run in a background thread, it fetches
  // for discounts for all active carts.
  static void FetchInBackground(
      std::unique_ptr<network::PendingSharedURLLoaderFactory> pending_factory,
      std::unique_ptr<CartDiscountFetcher> fetcher,
      AfterFetchingCallback after_fetching_callback,
      std::vector<CartDB::KeyAndValue> proto_pairs,
      const bool is_oauth_fetch,
      const std::string access_token_str);

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
