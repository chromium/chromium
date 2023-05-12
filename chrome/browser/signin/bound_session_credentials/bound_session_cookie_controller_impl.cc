// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_controller_impl.h"

#include <memory>

#include "base/debug/dump_without_crashing.h"
#include "base/functional/bind.h"
#include "base/time/time.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_observer.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_refresh_cookie_fetcher_impl.h"
#include "components/signin/public/base/signin_client.h"
#include "net/http/http_status_code.h"

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
  if (IsCookieFresh()) {
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
  if (IsCookieFresh()) {
    ResumeBlockedRequests();
  }
  delegate_->OnCookieExpirationDateChanged();
}

std::unique_ptr<BoundSessionRefreshCookieFetcher>
BoundSessionCookieControllerImpl::CreateRefreshCookieFetcher() const {
  return refresh_cookie_fetcher_factory_for_testing_.is_null()
             ? std::make_unique<BoundSessionRefreshCookieFetcherImpl>(client_)
             : refresh_cookie_fetcher_factory_for_testing_.Run(client_, url_,
                                                               cookie_name_);
}

bool BoundSessionCookieControllerImpl::IsCookieFresh() {
  return cookie_expiration_time() > base::Time::Now();
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
    BoundSessionRefreshCookieFetcher::Result result) {
  refresh_cookie_fetcher_.reset();
  if (result.net_error == net::OK && result.response_code == net::HTTP_OK) {
    // Requests are resumed once the cookie is set in the cookie jar. The
    // cookie is expected to be fresh and `this` is notified with its
    // expiration date before `OnCookieRefreshFetched` is called.
    if (IsCookieFresh()) {
      CHECK(resume_blocked_requests_.empty());
      return;
    } else {
      // The request should include `Set-Cookie` header. `this` is expected to
      // have been notified of the new cookie inserted in the cookie jar by the
      // time `OnCookieRefreshFetched()` is called.
      base::debug::DumpWithoutCrashing();
    }
  }
  // TODO(b/263263352): Handle error cases.
  ResumeBlockedRequests();
  NOTIMPLEMENTED();
}

void BoundSessionCookieControllerImpl::ResumeBlockedRequests() {
  std::vector<base::OnceClosure> callbacks;
  std::swap(callbacks, resume_blocked_requests_);
  for (auto& callback : callbacks) {
    std::move(callback).Run();
  }
}
