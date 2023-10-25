// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_controller_impl.h"

#include <memory>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_observer.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_refresh_cookie_fetcher.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_refresh_cookie_fetcher_impl.h"
#include "chrome/browser/signin/bound_session_credentials/session_binding_helper.h"
#include "chrome/browser/signin/wait_for_network_callback_helper_chrome.h"
#include "content/public/browser/storage_partition.h"
#include "services/network/public/cpp/network_connection_tracker.h"

namespace {
using Result = BoundSessionRefreshCookieFetcher::Result;
}

BoundSessionCookieControllerImpl::BoundSessionCookieControllerImpl(
    unexportable_keys::UnexportableKeyService& key_service,
    content::StoragePartition* storage_partition,
    network::NetworkConnectionTracker* network_connection_tracker,
    const bound_session_credentials::BoundSessionParams& bound_session_params,
    Delegate* delegate)
    : BoundSessionCookieController(bound_session_params, delegate),
      key_service_(key_service),
      storage_partition_(storage_partition),
      network_connection_tracker_(network_connection_tracker),
      wait_for_network_callback_helper_(
          std::make_unique<WaitForNetworkCallbackHelperChrome>()) {
  CHECK(!bound_session_params.wrapped_key().empty());
  base::span<const uint8_t> wrapped_key =
      base::as_bytes(base::make_span(bound_session_params.wrapped_key()));
  session_binding_helper_ = std::make_unique<SessionBindingHelper>(
      key_service_.get(), wrapped_key, session_id_);
  // Preemptively load the binding key to speed up the generation of binding
  // key assertion.
  session_binding_helper_->MaybeLoadBindingKey();
}

BoundSessionCookieControllerImpl::~BoundSessionCookieControllerImpl() {
  // On shutdown or session termination, resume blocked requests if any.
  ResumeBlockedRequests(
      ResumeBlockedRequestsTrigger::kShutdownOrSessionTermination);
}

void BoundSessionCookieControllerImpl::Initialize() {
  network_connection_observer_.Observe(network_connection_tracker_);
  CreateBoundCookiesObservers();
  MaybeRefreshCookie();
}

void BoundSessionCookieControllerImpl::OnConnectionChanged(
    network::mojom::ConnectionType type) {
  if (type == network::mojom::ConnectionType::CONNECTION_NONE) {
    // Let network requests fail now while there is no internet connection,
    // instead of holding them up until the network is back or timeout occurs.
    // The network could come back shortly before the timeout which would result
    // in requests being released without a valid cookie.
    ResumeBlockedRequests(
        ResumeBlockedRequestsTrigger::kNetworkConnectionOffline);
  }
}

bool BoundSessionCookieControllerImpl::IsConnectionTypeAvailableAndOffline() {
  network::mojom::ConnectionType type;
  return network_connection_tracker_->GetConnectionType(
             &type, base::BindOnce(
                        &BoundSessionCookieControllerImpl::OnConnectionChanged,
                        weak_ptr_factory_.GetWeakPtr())) &&
         type == network::mojom::ConnectionType::CONNECTION_NONE;
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

  if (IsConnectionTypeAvailableAndOffline()) {
    // See the comment in `OnConnectionChanged()` for explanation.
    ResumeBlockedRequests(
        ResumeBlockedRequestsTrigger::kNetworkConnectionOffline);
    return;
  }

  if (!resume_blocked_requests_timer_.IsRunning() &&
      !resume_blocked_requests_.empty()) {
    // Ensure all blocked requests are released after a timeout.
    // `base::Unretained(this)` is safe because `this` owns
    // `resume_blocked_requests_timer_`.
    const base::TimeDelta kResumeBlockedRequestTimeout = base::Seconds(20);
    resume_blocked_requests_timer_.Start(
        FROM_HERE, kResumeBlockedRequestTimeout,
        base::BindRepeating(
            &BoundSessionCookieControllerImpl::OnResumeBlockedRequestsTimeout,
            base::Unretained(this)));
  }
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
    ResumeBlockedRequests(ResumeBlockedRequestsTrigger::kObservedFreshCookies);
  }

  if (min_cookie_expiration_time() != old_min_expiration_time) {
    delegate_->OnBoundSessionThrottlerParamsChanged();
    MaybeScheduleCookieRotation();
  }
}

void BoundSessionCookieControllerImpl::CreateBoundCookiesObservers() {
  for (const auto& [cookie_name, _] : bound_cookies_info_) {
    // `base::Unretained(this)` is safe because `this` owns
    // `cookie_observer_`.
    std::unique_ptr<BoundSessionCookieObserver> cookie_observer =
        std::make_unique<BoundSessionCookieObserver>(
            storage_partition_, url_, cookie_name,
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
                   storage_partition_->GetURLLoaderFactoryForBrowserProcess(),
                   *wait_for_network_callback_helper_, *session_binding_helper_,
                   url_, std::move(cookie_names))
             : refresh_cookie_fetcher_factory_for_testing_.Run(
                   storage_partition_->GetCookieManagerForBrowserProcess(),
                   url_, std::move(cookie_names));
}

bool BoundSessionCookieControllerImpl::AreAllCookiesFresh() {
  return min_cookie_expiration_time() > base::Time::Now();
}

void BoundSessionCookieControllerImpl::MaybeRefreshCookie() {
  preemptive_cookie_refresh_timer_.Stop();
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

  ResumeBlockedRequestsTrigger trigger =
      result == BoundSessionRefreshCookieFetcher::Result::kSuccess
          ? ResumeBlockedRequestsTrigger::kCookieRefreshFetchSuccess
          : ResumeBlockedRequestsTrigger::kCookieRefreshFetchFailure;
  // Resume blocked requests regardless of the result.
  ResumeBlockedRequests(trigger);

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
  preemptive_cookie_refresh_timer_.Start(
      FROM_HERE, refresh_in,
      base::BindRepeating(&BoundSessionCookieControllerImpl::MaybeRefreshCookie,
                          base::Unretained(this)));
}

void BoundSessionCookieControllerImpl::ResumeBlockedRequests(
    ResumeBlockedRequestsTrigger trigger) {
  resume_blocked_requests_timer_.Stop();
  if (resume_blocked_requests_.empty()) {
    return;
  }
  std::vector<base::OnceClosure> callbacks;
  std::swap(callbacks, resume_blocked_requests_);
  for (auto& callback : callbacks) {
    std::move(callback).Run();
  }
  base::UmaHistogramEnumeration(
      "Signin.BoundSessionCredentials.ResumeThrottledRequestsTrigger", trigger);
}

void BoundSessionCookieControllerImpl::OnResumeBlockedRequestsTimeout() {
  // Reset the fetcher, it has been taking at least
  // kResumeBlockedRequestTimeout. New requests will trigger a new fetch.
  refresh_cookie_fetcher_.reset();
  ResumeBlockedRequests(ResumeBlockedRequestsTrigger::kTimeout);
}
