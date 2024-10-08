// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cart/fetch_discount_worker.h"

#include "base/memory/scoped_refptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/cart/cart_discount_fetcher.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/proto/coupon_db_content.pb.h"
#include "components/search/ntp_features.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/variations/variations.mojom.h"
#include "components/variations/variations_client.h"
#include "components/variations/variations_features.h"
#include "components/variations/variations_ids_provider.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/gaia/gaia_constants.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/re2/src/re2/re2.h"

namespace {
const char kOauthName[] = "rbd";
const char kOauthScopes[] = "https://www.googleapis.com/auth/chromememex";
const char kEmptyToken[] = "";
}  // namespace

CartDiscountServiceDelegate::CartDiscountServiceDelegate(
    CartService* cart_service)
    : cart_service_(cart_service) {}

CartDiscountServiceDelegate::~CartDiscountServiceDelegate() = default;

void CartDiscountServiceDelegate::LoadAllCarts(CartDB::LoadCallback callback) {
  cart_service_->LoadAllActiveCarts(std::move(callback));
}

void CartDiscountServiceDelegate::UpdateCart(
    const std::string& cart_url,
    const cart_db::ChromeCartContentProto new_proto,
    const bool is_tester) {
  cart_service_->UpdateDiscounts(GURL(cart_url), std::move(new_proto),
                                 is_tester);
}

void CartDiscountServiceDelegate::RecordFetchTimestamp() {
  cart_service_->RecordFetchTimestamp();
}

void CartDiscountServiceDelegate::UpdateFreeListingCoupons(
    const CouponService::CouponsMap& map) {
  cart_service_->UpdateFreeListingCoupons(map);
}

FetchDiscountWorker::FetchDiscountWorker(
    scoped_refptr<network::SharedURLLoaderFactory>
        browserProcessURLLoaderFactory,
    std::unique_ptr<CartDiscountFetcherFactory> fetcher_factory,
    std::unique_ptr<CartDiscountServiceDelegate> cart_discount_service_delegate,
    signin::IdentityManager* const identity_manager,
    variations::VariationsClient* const chrome_variations_client)
    : browserProcessURLLoaderFactory_(browserProcessURLLoaderFactory),
      fetcher_factory_(std::move(fetcher_factory)),
      cart_discount_service_delegate_(
          std::move(cart_discount_service_delegate)),
      identity_manager_(identity_manager),
      chrome_variations_client_(chrome_variations_client) {
  backend_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::TaskPriority::BEST_EFFORT});
}

FetchDiscountWorker::~FetchDiscountWorker() = default;

void FetchDiscountWorker::Start(base::TimeDelta delay) {
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
          signin::PrimaryAccountAccessTokenFetcher::Mode::kImmediate,
          signin::ConsentLevel::kSync);
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

void FetchDiscountWorker::LoadAllActiveCarts(bool is_oauth_fetch,
                                             std::string access_token_str) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  auto cart_loaded_callback = base::BindOnce(
      &FetchDiscountWorker::ReadyToFetch, weak_ptr_factory_.GetWeakPtr(),
      is_oauth_fetch, std::move(access_token_str));
  cart_discount_service_delegate_->LoadAllCarts(
      std::move(cart_loaded_callback));
}

void FetchDiscountWorker::ReadyToFetch(
    bool is_oauth_fetch,
    std::string access_token_str,
    bool success,
    std::vector<CartDB::KeyAndValue> proto_pairs) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  auto pending_factory = browserProcessURLLoaderFactory_->Clone();
  auto fetcher = fetcher_factory_->createFetcher();
  auto done_fetching_callback =
      base::BindOnce(&FetchDiscountWorker::AfterDiscountFetched,
                     weak_ptr_factory_.GetWeakPtr());

  cart_discount_service_delegate_->RecordFetchTimestamp();
  // If there is no eligible merchant cart, don't fetch immediately; instead,
  // post another delayed fetch.
  bool has_partner_merchant = false;
  bool has_potential_merchant = false;
  for (auto pair : proto_pairs) {
    auto cart_url = pair.second.merchant_cart_url();
    bool is_partner_merchant = commerce::IsPartnerMerchant(GURL(cart_url));
    bool is_potential_merchant =
        !commerce::IsNoDiscountMerchant(GURL(cart_url));
    has_partner_merchant |= is_partner_merchant;
    has_potential_merchant |= is_potential_merchant;
  }
  if (has_partner_merchant || has_potential_merchant) {
    backend_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &FetchInBackground, std::move(pending_factory), std::move(fetcher),
            std::move(done_fetching_callback), std::move(proto_pairs),
            is_oauth_fetch, std::move(access_token_str),
            g_browser_process->GetApplicationLocale(), GetVariationsHeaders()));
  } else {
    Start(commerce::GetDiscountFetchDelay());
  }
}

std::string FetchDiscountWorker::GetVariationsHeaders() {
  if (!chrome_variations_client_) {
    return "";
  }

  variations::mojom::VariationsHeadersPtr variations_headers =
      chrome_variations_client_->GetVariationsHeaders();

  if (variations_headers.is_null()) {
    return "";
  }

  return variations_headers->headers_map.at(
      variations::mojom::GoogleWebVisibility::FIRST_PARTY);
}

void FetchDiscountWorker::FetchInBackground(
    std::unique_ptr<network::PendingSharedURLLoaderFactory> pending_factory,
    std::unique_ptr<CartDiscountFetcher> fetcher,
    AfterFetchingCallback after_fetching_callback,
    std::vector<CartDB::KeyAndValue> proto_pairs,
    bool is_oauth_fetch,
    std::string access_token_str,
    std::string fetch_for_locale,
    std::string variation_headers) {
  DCHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  auto done_fetching_callback = base::BindOnce(
      &DoneFetchingInBackground, std::move(after_fetching_callback));
  fetcher->Fetch(std::move(pending_factory), std::move(done_fetching_callback),
                 std::move(proto_pairs), is_oauth_fetch,
                 std::move(access_token_str), std::move(fetch_for_locale),
                 std::move(variation_headers));
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
  cart_discount_service_delegate_->LoadAllCarts(
      std::move(update_discount_callback));
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

  double current_timestamp = base::Time::Now().InSecondsFSinceUnixEpoch();

  base::flat_map<GURL,
                 std::vector<std::unique_ptr<autofill::AutofillOfferData>>>
      coupon_map;
  for (CartDB::KeyAndValue& key_and_value : proto_pairs) {
    cart_db::ChromeCartContentProto cart_proto = key_and_value.second;
    std::string cart_url_str = cart_proto.merchant_cart_url();
    GURL cart_url_origin = GURL(cart_url_str).DeprecatedGetOriginAsURL();

    cart_db::ChromeCartDiscountProto* cart_discount_proto =
        cart_proto.mutable_discount_info();

    cart_discount_proto->set_last_fetched_timestamp(current_timestamp);

    if (!discounts.count(cart_url_str)) {
      cart_discount_proto->clear_discount_text();
      cart_discount_proto->clear_rule_discount_info();
      cart_discount_proto->clear_has_coupons();
      cart_discount_service_delegate_->UpdateCart(
          cart_url_str, std::move(cart_proto), is_tester);
      continue;
    }

    const MerchantIdAndDiscounts& merchant_discounts =
        discounts.at(cart_url_str);
    std::string merchant_id = merchant_discounts.merchant_id;
    cart_discount_proto->set_merchant_id(merchant_id);

    const std::vector<cart_db::RuleDiscountInfoProto>& discount_infos =
        merchant_discounts.rule_discounts;
    cart_discount_proto->set_discount_text(
        merchant_discounts.highest_discount_string);
    *cart_discount_proto->mutable_rule_discount_info() = {
        discount_infos.begin(), discount_infos.end()};
    cart_discount_proto->set_has_coupons(merchant_discounts.has_coupons);

    cart_discount_service_delegate_->UpdateCart(
        cart_url_str, std::move(cart_proto), is_tester);

    if (commerce::IsCouponWithCodeEnabled()) {
      for (const coupon_db::FreeListingCouponInfoProto& coupon_info :
           merchant_discounts.coupon_discounts) {
        int64_t offer_id = coupon_info.coupon_id();
        base::Time expiry =
            base::Time::FromSecondsSinceUnixEpoch(coupon_info.expiry_time());
        std::vector<GURL> merchant_origins;
        merchant_origins.emplace_back(cart_url_origin);
        GURL offer_details_url = GURL();
        autofill::DisplayStrings display_strings;
        display_strings.value_prop_text = coupon_info.coupon_description();
        std::string promo_code = coupon_info.coupon_code();

        auto offer = std::make_unique<autofill::AutofillOfferData>(
            autofill::AutofillOfferData::FreeListingCouponOffer(
                offer_id, expiry, merchant_origins, offer_details_url,
                display_strings, promo_code));
        coupon_map[cart_url_origin].emplace_back(std::move(offer));
      }
    }
  }

  if (commerce::IsCouponWithCodeEnabled()) {
    cart_discount_service_delegate_->UpdateFreeListingCoupons(coupon_map);
  }

  if (commerce::IsCartDiscountFeatureEnabled()) {
    // Continue to work.
    Start(commerce::GetDiscountFetchDelay());
  }
}
