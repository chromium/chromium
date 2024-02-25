// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/dice_bound_session_cookie_service.h"

#include "base/barrier_closure.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "components/signin/public/identity_manager/accounts_cookie_mutator.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/storage_partition.h"
#include "google_apis/gaia/gaia_urls.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"

DiceBoundSessionCookieService::DiceBoundSessionCookieService(
    BoundSessionCookieRefreshService& bound_session_cookie_refresh_service,
    signin::IdentityManager& identity_manager,
    content::StoragePartition& storage_partition)
    : identity_manager_(identity_manager),
      storage_partition_(storage_partition) {
  bound_session_cookie_refresh_service_observer_.Observe(
      &bound_session_cookie_refresh_service);
}

DiceBoundSessionCookieService::~DiceBoundSessionCookieService() = default;

void DiceBoundSessionCookieService::OnBoundSessionTerminated(
    const GURL& site,
    const base::flat_set<std::string>& bound_cookie_names) {
  const GURL gaia_url = GaiaUrls::GetInstance()->gaia_url();
  if (!gaia_url.DomainIs(site.host_piece())) {
    return;
  }

  network::mojom::CookieManager* cookie_manager =
      storage_partition_->GetCookieManagerForBrowserProcess();
  if (!cookie_manager) {
    TriggerCookieJarUpdate();
    return;
  }

  // TODO(b/314281485): Explicit cookie deletion triggers /ListAccounts.
  // Consider not triggering list accounts if at least one cookie is deleted.
  auto on_cookies_deleted = base::BarrierClosure(
      bound_cookie_names.size(),
      base::BindOnce(&DiceBoundSessionCookieService::TriggerCookieJarUpdate,
                     weak_ptr_factory_.GetWeakPtr()));
  for (const std::string& cookie_name : bound_cookie_names) {
    DeleteCookie(*cookie_manager, gaia_url, cookie_name, on_cookies_deleted);
  }
}

// static
void DiceBoundSessionCookieService::DeleteCookie(
    network::mojom::CookieManager& cookie_manager,
    const GURL& url,
    const std::string& cookie_name,
    base::OnceClosure on_cookie_deleted) {
  CHECK(on_cookie_deleted);
  network::mojom::CookieDeletionFilterPtr filter =
      network::mojom::CookieDeletionFilter::New();
  filter->url = url;
  filter->cookie_name = cookie_name;
  cookie_manager.DeleteCookies(
      std::move(filter),
      base::IgnoreArgs<uint32_t>(std::move(on_cookie_deleted)));
}

void DiceBoundSessionCookieService::TriggerCookieJarUpdate() {
  identity_manager_->GetAccountsCookieMutator()->TriggerCookieJarUpdate();
}
