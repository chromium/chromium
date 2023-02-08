// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_controller_impl.h"

#include <memory>

#include "chrome/browser/signin/bound_session_credentials/bound_session_refresh_cookie_fetcher.h"
#include "components/signin/public/base/signin_client.h"

BoundSessionCookieControllerImpl::BoundSessionCookieControllerImpl(
    SigninClient* client,
    const GURL& url,
    const std::string& cookie_name,
    Delegate* delegate)
    : BoundSessionCookieController(url, cookie_name, delegate),
      client_(client) {}

BoundSessionCookieControllerImpl::~BoundSessionCookieControllerImpl() = default;

void BoundSessionCookieControllerImpl::Initialize() {
  StartRefreshCookieRequest();
}

void BoundSessionCookieControllerImpl::SetCookieExpirationTimeAndNotify(
    const base::Time& expiration_time) {
  if (cookie_expiration_time_ == expiration_time) {
    return;
  }

  cookie_expiration_time_ = expiration_time;
  delegate_->OnCookieExpirationDateChanged();
}

std::unique_ptr<BoundSessionRefreshCookieFetcher>
BoundSessionCookieControllerImpl::CreateRefreshCookieFetcher() const {
  return refresh_cookie_fetcher_factory_for_testing_.is_null()
             ? std::make_unique<BoundSessionRefreshCookieFetcher>(client_, url_,
                                                                  cookie_name_)
             : refresh_cookie_fetcher_factory_for_testing_.Run(client_, url_,
                                                               cookie_name_);
}

void BoundSessionCookieControllerImpl::StartRefreshCookieRequest() {
  if (refresh_cookie_fetcher_) {
    return;
  }
  refresh_cookie_fetcher_ = CreateRefreshCookieFetcher();
  DCHECK(refresh_cookie_fetcher_);
  // `base::Unretained(this)` is safe because `this` owns
  // `refresh_cookie_fetcher_`.
  refresh_cookie_fetcher_->Start(
      base::BindOnce(&BoundSessionCookieControllerImpl::OnCookieRefreshFetched,
                     base::Unretained(this)));
}

void BoundSessionCookieControllerImpl::OnCookieRefreshFetched(
    absl::optional<const base::Time> expiration_time) {
  refresh_cookie_fetcher_.reset();
  if (expiration_time.has_value()) {
    // Cookie fetch succeeded.
    // We do not check for null time and honor the expiration date of the cookie
    // sent by the server.
    SetCookieExpirationTimeAndNotify(expiration_time.value());
  }
  // TODO(msalama): Handle error cases.
}
