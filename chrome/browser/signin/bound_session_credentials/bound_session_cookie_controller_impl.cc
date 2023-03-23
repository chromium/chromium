// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_controller_impl.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/time/time.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_observer.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_refresh_cookie_fetcher.h"
#include "components/signin/public/base/signin_client.h"

BoundSessionCookieControllerImpl::BoundSessionCookieControllerImpl(
    SigninClient* client,
    const GURL& url,
    const std::string& cookie_name,
    Delegate* delegate)
    : BoundSessionCookieController(url, cookie_name, delegate),
      client_(client) {}

BoundSessionCookieControllerImpl::~BoundSessionCookieControllerImpl() {
  // On shutdown or session termination, resume blocked requests if any.
  ResumeBlockedRequests();
}

void BoundSessionCookieControllerImpl::Initialize() {
  // `base::Unretained(this)` is safe because `this` owns
  // `cookie_observer_`.
  cookie_observer_ = std::make_unique<BoundSessionCookieObserver>(
      client_, url_, cookie_name_,
      base::BindRepeating(
          &BoundSessionCookieControllerImpl::SetCookieExpirationTimeAndNotify,
          base::Unretained(this)));
  MaybeRefreshCookie();
}

void BoundSessionCookieControllerImpl::OnRequestBlockedOnCookie(
    base::OnceClosure resume_blocked_request) {
  if (cookie_expiration_time_ > base::Time::Now()) {
    // Cookie is fresh.
    std::move(resume_blocked_request).Run();
    return;
  }

  resume_blocked_requests_.push_back(std::move(resume_blocked_request));
  MaybeRefreshCookie();
}

void BoundSessionCookieControllerImpl::SetCookieExpirationTimeAndNotify(
    base::Time expiration_time) {
  if (cookie_expiration_time_ == expiration_time) {
    return;
  }

  // TODO(b/263264391): Subtract a safety margin (e.g 2 seconds) from the cookie
  // expiration time.
  cookie_expiration_time_ = expiration_time;
  if (cookie_expiration_time_ > base::Time::Now()) {
    ResumeBlockedRequests();
  }
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

void BoundSessionCookieControllerImpl::MaybeRefreshCookie() {
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
    absl::optional<base::Time> expiration_time) {
  refresh_cookie_fetcher_.reset();
  if (expiration_time.has_value()) {
    // Cookie fetch succeeded.
    // We do not check for null time and honor the expiration date of the cookie
    // sent by the server.
    // TODO(b/263264391): Rely on the observer notification to complete the
    // request.
    if (expiration_time.value() < base::Time::Now()) {
      NOTIMPLEMENTED();
    }
    SetCookieExpirationTimeAndNotify(expiration_time.value());
  } else {
    ResumeBlockedRequests();
  }
  // TODO(b/263263352): Handle error cases.
}

void BoundSessionCookieControllerImpl::ResumeBlockedRequests() {
  std::vector<base::OnceClosure> callbacks;
  std::swap(callbacks, resume_blocked_requests_);
  for (auto& callback : callbacks) {
    std::move(callback).Run();
  }
}
