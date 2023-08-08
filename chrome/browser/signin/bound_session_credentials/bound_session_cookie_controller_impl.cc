// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_controller_impl.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/time/time.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_observer.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_refresh_cookie_fetcher.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_refresh_cookie_fetcher_impl.h"
#include "chrome/browser/signin/bound_session_credentials/session_binding_helper.h"
#include "chrome/browser/signin/wait_for_network_callback_helper_chrome.h"
#include "components/signin/public/base/signin_client.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace {
using Result = BoundSessionRefreshCookieFetcher::Result;
}

BoundSessionCookieControllerImpl::BoundSessionCookieControllerImpl(
    unexportable_keys::UnexportableKeyService& key_service,
    SigninClient* client,
    bound_session_credentials::RegistrationParams registration_params,
    const base::flat_set<std::string>& cookie_names,
    Delegate* delegate)
    : BoundSessionCookieController(registration_params, cookie_names, delegate),
      key_service_(key_service),
      client_(client),
      wait_for_network_callback_helper_(
          std::make_unique<WaitForNetworkCallbackHelperChrome>()) {
  // TODO(b/273920907): Mark `wrapped_key` as non-optional when
  // `BoundSessionCookieRefreshServiceImpl` uses only
  // explicitly registered sessions.
  base::span<const uint8_t> wrapped_key =
      base::as_bytes(base::make_span(registration_params.wrapped_key()));
  if (!wrapped_key.empty()) {
    session_binding_helper_ = std::make_unique<SessionBindingHelper>(
        key_service_.get(), wrapped_key, /*session_id=*/"");
    // Preemptively load the binding key to speed up the generation of binding
    // key assertion.
    session_binding_helper_->MaybeLoadBindingKey();
  }
}

BoundSessionCookieControllerImpl::~BoundSessionCookieControllerImpl() {
  // On shutdown or session termination, resume blocked requests if any.
  ResumeBlockedRequests();
}

void BoundSessionCookieControllerImpl::Initialize() {
  CreateBoundCookiesObservers();
  MaybeRefreshCookie();
}

void BoundSessionCookieControllerImpl::OnRequestBlockedOnCookie(
    base::OnceClosure resume_blocked_request) {
  if (AreAllCookiesFresh()) {
    // Cookie is fresh.
    std::move(resume_blocked_request).Run();
    return;
  }

  resume_blocked_requests_.push_back(std::move(resume_blocked_request));
  MaybeRefreshCookie();
}

void BoundSessionCookieControllerImpl::SetCookieExpirationTimeAndNotify(
    const std::string& cookie_name,
    base::Time expiration_time) {
  const base::TimeDelta kCookieExpirationThreshold = base::Seconds(15);
  if (!expiration_time.is_null()) {
    expiration_time -= kCookieExpirationThreshold;
  }

  auto it = bound_cookies_info_.find(cookie_name);
  CHECK(it != bound_cookies_info_.end());
  if (it->second == expiration_time) {
    return;
  }

  base::Time old_min_expiration_time = min_cookie_expiration_time();
  it->second = expiration_time;
  if (AreAllCookiesFresh()) {
    ResumeBlockedRequests();
  }

  if (min_cookie_expiration_time() != old_min_expiration_time) {
    delegate_->OnBoundSessionParamsChanged();
    MaybeScheduleCookieRotation();
  }
}

void BoundSessionCookieControllerImpl::CreateBoundCookiesObservers() {
  for (const auto& [cookie_name, _] : bound_cookies_info_) {
    // `base::Unretained(this)` is safe because `this` owns
    // `cookie_observer_`.
    std::unique_ptr<BoundSessionCookieObserver> cookie_observer =
        std::make_unique<BoundSessionCookieObserver>(
            client_, url_, cookie_name,
            base::BindRepeating(&BoundSessionCookieControllerImpl::
                                    SetCookieExpirationTimeAndNotify,
                                base::Unretained(this)));
    bound_cookies_observers_.push_back(std::move(cookie_observer));
  }
}

std::unique_ptr<BoundSessionRefreshCookieFetcher>
BoundSessionCookieControllerImpl::CreateRefreshCookieFetcher() const {
  base::flat_set<std::string> cookie_names;
  for (const auto& [cookie_name, _] : bound_cookies_info_) {
    cookie_names.insert(cookie_name);
  }

  return refresh_cookie_fetcher_factory_for_testing_.is_null()
             ? std::make_unique<BoundSessionRefreshCookieFetcherImpl>(
                   client_->GetURLLoaderFactory(),
                   *wait_for_network_callback_helper_, *session_binding_helper_,
                   url_, std::move(cookie_names))
             : refresh_cookie_fetcher_factory_for_testing_.Run(
                   client_->GetCookieManager(), url_, std::move(cookie_names));
}

bool BoundSessionCookieControllerImpl::AreAllCookiesFresh() {
  return min_cookie_expiration_time() > base::Time::Now();
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

  if (BoundSessionRefreshCookieFetcher::IsPersistentError(result)) {
    delegate_->TerminateSession();
    // `this` should be deleted.
  }
}

void BoundSessionCookieControllerImpl::MaybeScheduleCookieRotation() {
  const base::TimeDelta kCookieRefreshInterval = base::Minutes(2);
  base::TimeDelta refresh_in =
      min_cookie_expiration_time() - base::Time::Now() - kCookieRefreshInterval;
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
