// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_controller_impl.h"

#include <memory>

#include "base/debug/dump_without_crashing.h"
#include "base/functional/bind.h"
#include "base/time/time.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_observer.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_refresh_cookie_fetcher.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_refresh_cookie_fetcher_impl.h"
#include "components/signin/public/base/signin_client.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"

BoundSessionCookieControllerImpl::BoundSessionCookieControllerImpl(
    SigninClient* client,
    const GURL& url,
    const std::vector<std::string>& cookie_names,
    Delegate* delegate)
    : BoundSessionCookieController(url, cookie_names, delegate),
      client_(client) {}

BoundSessionCookieControllerImpl::~BoundSessionCookieControllerImpl() {
  // On shutdown or session termination, resume blocked requests if any.
  ResumeBlockedRequests();
}

void BoundSessionCookieControllerImpl::Initialize() {
  // `base::Unretained(this)` is safe because `this` owns
  // `cookie_observer_`.
  cookie_observer_ = std::make_unique<BoundSessionCookieObserver>(
      client_, url_, cookie_name(),
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
  const base::TimeDelta kCookieExpirationThreshold = base::Seconds(15);
  if (!expiration_time.is_null()) {
    expiration_time -= kCookieExpirationThreshold;
  }

  if (cookie_expiration_time() == expiration_time) {
    return;
  }

  bound_cookies_info_.begin()->second = expiration_time;
  if (IsCookieFresh()) {
    ResumeBlockedRequests();
  }
  delegate_->OnCookieExpirationDateChanged();
  MaybeScheduleCookieRotation();
}

std::unique_ptr<BoundSessionRefreshCookieFetcher>
BoundSessionCookieControllerImpl::CreateRefreshCookieFetcher() const {
  return refresh_cookie_fetcher_factory_for_testing_.is_null()
             ? std::make_unique<BoundSessionRefreshCookieFetcherImpl>(client_)
             : refresh_cookie_fetcher_factory_for_testing_.Run(client_, url_,
                                                               cookie_name());
}

bool BoundSessionCookieControllerImpl::IsCookieFresh() {
  return cookie_expiration_time() > base::Time::Now();
}

void BoundSessionCookieControllerImpl::MaybeRefreshCookie() {
  cookie_refresh_timer_.Stop();
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
  // TODO(b/263263352): Record histogram with the result of the fetch.
  refresh_cookie_fetcher_.reset();

  // Resume blocked requests regardless of the result.
  ResumeBlockedRequests();

  // Persistent errors result in session termination.
  // Transient errors have no impact on future requests.
  if (result ==
      BoundSessionRefreshCookieFetcher::Result::kServerPersistentError) {
    delegate_->TerminateSession();
    // `this` should be deleted.
  }
}

void BoundSessionCookieControllerImpl::MaybeScheduleCookieRotation() {
  const base::TimeDelta kCookieRefreshInterval = base::Minutes(2);
  base::TimeDelta refresh_in =
      cookie_expiration_time() - base::Time::Now() - kCookieRefreshInterval;
  if (!refresh_in.is_positive()) {
    MaybeRefreshCookie();
    return;
  }

  // If a refresh task is already scheduled, this will reschedule it.
  // `base::Unretained(this)` is safe because `this` owns
  // `cookie_rotation_timer_`.
  cookie_refresh_timer_.Start(
      FROM_HERE, refresh_in,
      base::BindRepeating(&BoundSessionCookieControllerImpl::MaybeRefreshCookie,
                          base::Unretained(this)));
}

void BoundSessionCookieControllerImpl::ResumeBlockedRequests() {
  std::vector<base::OnceClosure> callbacks;
  std::swap(callbacks, resume_blocked_requests_);
  for (auto& callback : callbacks) {
    std::move(callback).Run();
  }
}
