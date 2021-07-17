// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cart/fetch_discount_worker.h"

#include "base/memory/scoped_refptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/cart/cart_discount_fetcher.h"
#include "components/search/ntp_features.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/gaia/gaia_constants.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace {
// Default value is 6 hours.
constexpr base::FeatureParam<base::TimeDelta> kDelayFetchParam(
    &ntp_features::kNtpChromeCartModule,
    "delay-fetch-discount",
    base::TimeDelta::FromHours(6));

const char kOauthName[] = "rbd";
const char kOauthScopes[] = "https://www.googleapis.com/auth/chromememex";
const char kEmptyToken[] = "";
}  // namespace

CartLoader::CartLoader(Profile* profile)
    : cart_service_(CartServiceFactory::GetForProfile(profile)) {}

CartLoader::~CartLoader() = default;

void CartLoader::LoadAllCarts(CartDB::LoadCallback callback) {
  cart_service_->LoadAllActiveCarts(std::move(callback));
}

CartDiscountUpdater::CartDiscountUpdater(Profile* profile)
    : cart_service_(CartServiceFactory::GetForProfile(profile)) {}

CartDiscountUpdater::~CartDiscountUpdater() = default;

void CartDiscountUpdater::update(
    const std::string& cart_url,
    const cart_db::ChromeCartContentProto new_proto,
    const bool is_tester) {
  GURL url(cart_url);
  cart_service_->UpdateDiscounts(url, std::move(new_proto), is_tester);
}

CartLoaderAndUpdaterFactory::CartLoaderAndUpdaterFactory(Profile* profile)
    : profile_(profile) {}

CartLoaderAndUpdaterFactory::~CartLoaderAndUpdaterFactory() = default;

std::unique_ptr<CartLoader> CartLoaderAndUpdaterFactory::createCartLoader() {
  return std::make_unique<CartLoader>(profile_);
}

std::unique_ptr<CartDiscountUpdater>
CartLoaderAndUpdaterFactory::createCartDiscountUpdater() {
  return std::make_unique<CartDiscountUpdater>(profile_);
}

FetchDiscountWorker::FetchDiscountWorker(
    scoped_refptr<network::SharedURLLoaderFactory>
        browserProcessURLLoaderFactory,
    std::unique_ptr<CartDiscountFetcherFactory> fetcher_factory,
    std::unique_ptr<CartLoaderAndUpdaterFactory>
        cart_loader_and_updater_factory,
    signin::IdentityManager* const identity_manager)
    : browserProcessURLLoaderFactory_(browserProcessURLLoaderFactory),
      fetcher_factory_(std::move(fetcher_factory)),
      cart_loader_and_updater_factory_(
          std::move(cart_loader_and_updater_factory)),
      identity_manager_(identity_manager) {
  backend_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::TaskPriority::BEST_EFFORT});
}

FetchDiscountWorker::~FetchDiscountWorker() = default;

void FetchDiscountWorker::Start(base::TimeDelta delay) {
  // Post a delay task to avoid an infinite loop for creating the CartService.
  // Since CartLoader and CartDiscountUpdater both depend on CartService.
  content::GetUIThreadTaskRunner({base::TaskPriority::BEST_EFFORT})
      ->PostDelayedTask(FROM_HERE,
                        base::BindOnce(&FetchDiscountWorker::PrepareToFetch,
                                       weak_ptr_factory_.GetWeakPtr()),
                        delay);
}

void FetchDiscountWorker::PrepareToFetch() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  if (identity_manager_ &&
      identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSync)) {
    FetchOauthToken();
  } else {
    LoadAllActiveCarts(/*is_oauth_fetch*/ false, kEmptyToken);
  }
}

void FetchDiscountWorker::FetchOauthToken() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(!access_token_fetcher_);

  signin::AccessTokenFetcher::TokenCallback token_callback = base::BindOnce(
      &FetchDiscountWorker::OnAuthTokenFetched, weak_ptr_factory_.GetWeakPtr());
  access_token_fetcher_ =
      std::make_unique<signin::PrimaryAccountAccessTokenFetcher>(
          kOauthName, identity_manager_, signin::ScopeSet{kOauthScopes},
          std::move(token_callback),
          signin::PrimaryAccountAccessTokenFetcher::Mode::kImmediate);
}

void FetchDiscountWorker::OnAuthTokenFetched(
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  access_token_fetcher_.reset();

  if (error.state() != GoogleServiceAuthError::NONE) {
    VLOG(3) << "There was an authentication error: " << error.state();
    return;
  }

  LoadAllActiveCarts(/*is_oauth_fetch*/ true, access_token_info.token);
}

void FetchDiscountWorker::LoadAllActiveCarts(
    const bool is_oauth_fetch,
    const std::string access_token_str) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  auto cart_loaded_callback = base::BindOnce(
      &FetchDiscountWorker::ReadyToFetch, weak_ptr_factory_.GetWeakPtr(),
      is_oauth_fetch, std::move(access_token_str));
  auto loader = cart_loader_and_updater_factory_->createCartLoader();
  loader->LoadAllCarts(std::move(cart_loaded_callback));
}

void FetchDiscountWorker::ReadyToFetch(
    const bool is_oauth_fetch,
    const std::string access_token_str,
    bool success,
    std::vector<CartDB::KeyAndValue> proto_pairs) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  auto pending_factory = browserProcessURLLoaderFactory_->Clone();
  auto fetcher = fetcher_factory_->createFetcher();
  auto done_fetching_callback =
      base::BindOnce(&FetchDiscountWorker::AfterDiscountFetched,
                     weak_ptr_factory_.GetWeakPtr());

  backend_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&FetchInBackground, std::move(pending_factory),
                     std::move(fetcher), std::move(done_fetching_callback),
                     std::move(proto_pairs), is_oauth_fetch,
                     std::move(access_token_str)));
}

void FetchDiscountWorker::FetchInBackground(
    std::unique_ptr<network::PendingSharedURLLoaderFactory> pending_factory,
    std::unique_ptr<CartDiscountFetcher> fetcher,
    AfterFetchingCallback after_fetching_callback,
    std::vector<CartDB::KeyAndValue> proto_pairs,
    const bool is_oauth_fetch,
    const std::string access_token_str) {
  DCHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  auto done_fetching_callback = base::BindOnce(
      &DoneFetchingInBackground, std::move(after_fetching_callback));
  fetcher->Fetch(std::move(pending_factory), std::move(done_fetching_callback),
                 std::move(proto_pairs), is_oauth_fetch,
                 std::move(access_token_str));
}

// TODO(meiliang): Follow up to use BindPostTask.
void FetchDiscountWorker::DoneFetchingInBackground(
    AfterFetchingCallback after_fetching_callback,
    CartDiscountFetcher::CartDiscountMap discounts,
    bool is_tester) {
  DCHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  content::GetUIThreadTaskRunner({base::TaskPriority::BEST_EFFORT})
      ->PostTask(FROM_HERE,
                 base::BindOnce(
                     [](AfterFetchingCallback callback,
                        CartDiscountFetcher::CartDiscountMap map, bool tester) {
                       std::move(callback).Run(std::move(map), tester);
                     },
                     std::move(after_fetching_callback), std::move(discounts),
                     is_tester));
}

void FetchDiscountWorker::AfterDiscountFetched(
    CartDiscountFetcher::CartDiscountMap discounts,
    bool is_tester) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  auto update_discount_callback = base::BindOnce(
      &FetchDiscountWorker::OnUpdatingDiscounts, weak_ptr_factory_.GetWeakPtr(),
      std::move(discounts), is_tester);
  auto loader = cart_loader_and_updater_factory_->createCartLoader();
  loader->LoadAllCarts(std::move(update_discount_callback));
}

void FetchDiscountWorker::OnUpdatingDiscounts(
    CartDiscountFetcher::CartDiscountMap discounts,
    bool is_tester,
    bool success,
    std::vector<CartDB::KeyAndValue> proto_pairs) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (!success || !proto_pairs.size()) {
    return;
  }

  auto updater = cart_loader_and_updater_factory_->createCartDiscountUpdater();

  double current_timestamp = base::Time::Now().ToDoubleT();

  for (CartDB::KeyAndValue& key_and_value : proto_pairs) {
    cart_db::ChromeCartContentProto cart_proto = key_and_value.second;
    std::string cart_url = cart_proto.merchant_cart_url();

    cart_db::ChromeCartDiscountProto* cart_discount_proto =
        cart_proto.mutable_discount_info();

    cart_discount_proto->set_last_fetched_timestamp(current_timestamp);

    if (!discounts.count(cart_url)) {
      cart_discount_proto->clear_discount_text();
      cart_discount_proto->clear_discount_info();
      updater->update(cart_url, std::move(cart_proto), is_tester);
      continue;
    }

    const MerchantIdAndDiscounts& merchant_discounts = discounts.at(cart_url);
    std::string merchant_id = merchant_discounts.merchant_id;
    cart_discount_proto->set_merchant_id(merchant_id);

    const std::vector<cart_db::DiscountInfoProto>& discount_infos =
        merchant_discounts.discount_list;
    cart_discount_proto->set_discount_text(
        merchant_discounts.highest_discount_string);
    *cart_discount_proto->mutable_discount_info() = {discount_infos.begin(),
                                                     discount_infos.end()};

    updater->update(cart_url, std::move(cart_proto), is_tester);
  }

  if (base::GetFieldTrialParamByFeatureAsBool(
          ntp_features::kNtpChromeCartModule,
          ntp_features::kNtpChromeCartModuleAbandonedCartDiscountParam,
          false)) {
    // Continue to work
    Start(kDelayFetchParam.Get());
  }
}
