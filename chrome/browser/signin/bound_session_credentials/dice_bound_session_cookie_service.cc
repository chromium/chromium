// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/dice_bound_session_cookie_service.h"

#include "components/signin/public/identity_manager/accounts_cookie_mutator.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "google_apis/gaia/gaia_urls.h"

DiceBoundSessionCookieService::DiceBoundSessionCookieService(
    BoundSessionCookieRefreshService& bound_session_cookie_refresh_service,
    signin::IdentityManager& identity_manager)
    : identity_manager_(identity_manager) {
  bound_session_cookie_refresh_service_observer_.Observe(
      &bound_session_cookie_refresh_service);
}

DiceBoundSessionCookieService::~DiceBoundSessionCookieService() = default;

void DiceBoundSessionCookieService::OnBoundSessionTerminated(
    const GURL& site,
    const base::flat_set<std::string>& bound_cookie_names) {
  if (!GaiaUrls::GetInstance()->gaia_url().DomainIs(site.host_piece())) {
    return;
  }
  // TODO(b/300627729): Ensure bound cookies are expired.
  // The Gaia cookie might not be sufficiently authorized after bound session
  // termination on the client, force trigger /ListAccounts.
  identity_manager_->GetAccountsCookieMutator()->TriggerCookieJarUpdate();
}
