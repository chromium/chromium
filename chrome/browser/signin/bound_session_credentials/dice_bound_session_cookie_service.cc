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

namespace {

void DeleteCookie(network::mojom::CookieManager& cookie_manager,
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

}  // namespace

DiceBoundSessionCookieService::DiceBoundSessionCookieService(
    BoundSessionCookieRefreshService& bound_session_cookie_refresh_service,
    signin::IdentityManager& identity_manager,
    content::StoragePartition& storage_partition)
    : identity_manager_(identity_manager),
      storage_partition_(storage_partition) {
  bound_session_cookie_refresh_service_observer_.Observe(
      &bound_session_cookie_refresh_service);
  network::mojom::DeviceBoundSessionManager* device_bound_session_manager =
      storage_partition_->GetDeviceBoundSessionManager();
  if (device_bound_session_manager) {
    device_bound_session_manager->AddObserver(
        GaiaUrls::GetInstance()->gaia_url(),
        receiver_.BindNewPipeAndPassRemote());
  }
}

DiceBoundSessionCookieService::~DiceBoundSessionCookieService() = default;

void DiceBoundSessionCookieService::OnBoundSessionTerminated(
    const GURL& site,
    const base::flat_set<std::string>& bound_cookie_names) {
  const GURL gaia_url = GaiaUrls::GetInstance()->gaia_url();
  if (!gaia_url.DomainIs(site.host_piece())) {
    return;
  }

  DeleteCookies(bound_cookie_names);
}

void DiceBoundSessionCookieService::OnDeviceBoundSessionAccessed(
    const net::device_bound_sessions::SessionAccess& access) {
  if (access.access_type !=
      net::device_bound_sessions::SessionAccess::AccessType::kTermination) {
    return;
  }

  if (access.cookies.empty()) {
    TriggerCookieJarUpdate();
    return;
  }

  DeleteCookies(base::flat_set<std::string>(access.cookies.begin(),
                                            access.cookies.end()));
}

void DiceBoundSessionCookieService::Clone(
    mojo::PendingReceiver<network::mojom::DeviceBoundSessionAccessObserver>
        observer) {
  // The `Clone` method is only called for the observers that are part
  // of network requests, so it is not expected to be called here.
  NOTREACHED();
}

void DiceBoundSessionCookieService::DeleteCookies(
    const base::flat_set<std::string>& bound_cookie_names) {
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
  const GURL gaia_url = GaiaUrls::GetInstance()->gaia_url();
  for (const std::string& cookie_name : bound_cookie_names) {
    DeleteCookie(*cookie_manager, gaia_url, cookie_name, on_cookies_deleted);
  }
}

void DiceBoundSessionCookieService::TriggerCookieJarUpdate() {
  identity_manager_->GetAccountsCookieMutator()->TriggerCookieJarUpdate();
}
