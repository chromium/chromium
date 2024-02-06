// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/bound_session_oauth_multilogin_delegate_impl.h"

#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_refresh_service.h"
#include "google_apis/gaia/oauth_multilogin_result.h"

BoundSessionOAuthMultiLoginDelegateImpl::
    BoundSessionOAuthMultiLoginDelegateImpl(
        base::WeakPtr<BoundSessionCookieRefreshService>
            bound_session_cookie_refresh_service)
    : bound_session_cookie_refresh_service_(
          bound_session_cookie_refresh_service) {}

BoundSessionOAuthMultiLoginDelegateImpl::
    ~BoundSessionOAuthMultiLoginDelegateImpl() = default;

void BoundSessionOAuthMultiLoginDelegateImpl::BeforeSetCookies(
    const OAuthMultiloginResult& result) {
  // TODO(msalama): Check `result` for `DbscMetaData`and call
  // `bound_session_cookie_service_` accordingly.
  // This could result in pausing cookie rotation for Google bound sessions.
  // Requests waiting on cookies must remain blocked till cookies are set.
}

void BoundSessionOAuthMultiLoginDelegateImpl::OnCookiesSet() {
  // TODO(msalama): Start/Override Google DBSC session if needed.
}
