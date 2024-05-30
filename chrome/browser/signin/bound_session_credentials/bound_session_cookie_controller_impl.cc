// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_controller_impl.h"

#include <memory>
#include <optional>
#include <variant>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/overloaded.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_observer.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_params_util.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_refresh_cookie_fetcher.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_refresh_cookie_fetcher_impl.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_switches.h"
#include "chrome/browser/signin/bound_session_credentials/rotation_debug_info.pb.h"
#include "chrome/browser/signin/bound_session_credentials/session_binding_helper.h"
#include "chrome/common/renderer_configuration.mojom.h"
#include "content/public/browser/storage_partition.h"
#include "services/network/public/cpp/network_connection_tracker.h"

namespace {
using Result = BoundSessionRefreshCookieFetcher::Result;

void RecordNumberOfSuccessiveTimeoutIfAny(size_t successive_timeout) {
  if (successive_timeout == 0) {
    return;
  }

  base::UmaHistogramCounts100(
      "Signin.BoundSessionCredentials.ThrottledRequestsSuccessiveTimeout",
      successive_timeout);
}

struct TimeoutOccured {};

void UpdateDebugInfo(bound_session_credentials::RotationDebugInfo& info,
                     std::variant<Result, TimeoutOccured> last_result,
                     bool last_challenge_received) {
  using bound_session_credentials::RotationDebugInfo;
  // Null value means no error.
  std::optional<RotationDebugInfo::FailureType> failure_type = std::visit(
      base::Overloaded{
          [](Result result) -> std::optional<RotationDebugInfo::FailureType> {
            switch (result) {
              case Result::kConnectionError:
                return RotationDebugInfo::CONNECTION_ERROR;
              case Result::kServerTransientError:
                return RotationDebugInfo::SERVER_ERROR;
              case Result::kSuccess:
                return std::nullopt;
              default:
                return RotationDebugInfo::OTHER;
            }
          },
          [](TimeoutOccured) -> std::optional<RotationDebugInfo::FailureType> {
            return RotationDebugInfo::TIMEOUT;
          }},
      last_result);

  if (!failure_type.has_value()) {
    // Clear `info` on success.
    info.Clear();
    return;
  }

  auto counter_it = base::ranges::find_if(
      *info.mutable_errors_since_last_rotation(),
      [&failure_type](const RotationDebugInfo::FailureCounter& counter) {
        return counter.type() == failure_type.value();
      });
  if (counter_it == info.errors_since_last_rotation().end()) {
    RotationDebugInfo::FailureCounter* counter =
        info.add_errors_since_last_rotation();
    counter->set_type(failure_type.value());
    counter->set_count(1);
  } else {
    counter_it->set_count(counter_it->count() + 1);
  }

  if (!info.has_first_failure_info()) {
    RotationDebugInfo::FailureInfo* failure_info =
        info.mutable_first_failure_info();
    *failure_info->mutable_failure_time() =
        bound_session_credentials::TimeToTimestamp(base::Time::Now());
    failure_info->set_type(failure_type.value());
    failure_info->set_received_challenge(last_challenge_received);
  }
}

}  // namespace

BoundSessionCookieControllerImpl::BoundSessionCookieControllerImpl(
    unexportable_keys::UnexportableKeyService& key_service,
    content::StoragePartition* storage_partition,
    network::NetworkConnectionTracker* network_connection_tracker,
    const bound_session_credentials::BoundSessionParams& bound_session_params,
    Delegate* delegate,
    bool is_off_the_record_profile)
    : BoundSessionCookieController(bound_session_params, delegate),
      key_service_(key_service),
      storage_partition_(storage_partition),
      network_connection_tracker_(network_connection_tracker),
      is_off_the_record_profile_(is_off_the_record_profile) {
  CHECK(!bound_session_params.wrapped_key().empty());
  base::span<const uint8_t> wrapped_key =
      base::as_bytes(base::make_span(bound_session_params.wrapped_key()));
  session_binding_helper_ = std::make_unique<SessionBindingHelper>(
      key_service_.get(), wrapped_key, session_id_);
  // Preemptively load the binding key to speed up the generation of binding
  // key assertion.
  session_binding_helper_->MaybeLoadBindingKey();

  std::optional<base::TimeDelta> cookie_rotation_delay =
      bound_session_credentials::GetCookieRotationDelayIfSetByCommandLine();

  if (cookie_rotation_delay) {
    // `base::Unretained(this)` is safe because `this` owns
    // `artifical_cookie_rotation_delay_`.
    artifical_cookie_rotation_delay_ =
        std::make_unique<base::RetainingOneShotTimer>(
            FROM_HERE, *cookie_rotation_delay,
            base::BindRepeating(
                &BoundSessionCookieControllerImpl::StartCookieRefresh,
                base::Unretained(this)));
  }

  artificial_cookie_rotation_result_ =
      bound_session_credentials::GetCookieRotationResultIfSetByCommandLine();
}

BoundSessionCookieControllerImpl::~BoundSessionCookieControllerImpl() {
  // On shutdown or session termination, resume blocked requests if any.
  ResumeBlockedRequests(chrome::mojom::ResumeBlockedRequestsTrigger::
                            kShutdownOrSessionTermination);
  RecordNumberOfSuccessiveTimeoutIfAny(successive_timeout_);
}

void BoundSessionCookieControllerImpl::Initialize() {
  network_connection_observer_.Observe(network_connection_tracker_);
  is_offline_ = network_connection_tracker_->IsOffline();
  CreateBoundCookiesObservers();
  MaybeRefreshCookie();
}

void BoundSessionCookieControllerImpl::OnConnectionChanged(
    network::mojom::ConnectionType type) {
  if (is_offline_ && type != network::mojom::ConnectionType::CONNECTION_NONE) {
    // We are back online. Schedule a new cookie rotation if needed.
    MaybeScheduleCookieRotation();
  }

  is_offline_ = type == network::mojom::ConnectionType::CONNECTION_NONE;
}

void BoundSessionCookieControllerImpl::HandleRequestBlockedOnCookie(
    chrome::mojom::BoundSessionRequestThrottledHandler::
        HandleRequestBlockedOnCookieCallback resume_blocked_request) {
  if (AreAllCookiesFresh()) {
    // Cookie is fresh.
    std::move(resume_blocked_request)
        .Run(chrome::mojom::ResumeBlockedRequestsTrigger::kCookieAlreadyFresh);
    return;
  }

  resume_blocked_requests_.push_back(std::move(resume_blocked_request));
  MaybeRefreshCookie();

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
    ResumeBlockedRequests(
        chrome::mojom::ResumeBlockedRequestsTrigger::kObservedFreshCookies);
    RecordNumberOfSuccessiveTimeoutIfAny(successive_timeout_);
    successive_timeout_ = 0;
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
  return refresh_cookie_fetcher_factory_for_testing_.is_null()
             ? std::make_unique<BoundSessionRefreshCookieFetcherImpl>(
                   storage_partition_->GetURLLoaderFactoryForBrowserProcess(),
                   *session_binding_helper_, session_id_, refresh_url_, url_,
                   bound_cookie_names(), is_off_the_record_profile_,
                   debug_info_)
             : refresh_cookie_fetcher_factory_for_testing_.Run(
                   storage_partition_->GetCookieManagerForBrowserProcess(),
                   url_, bound_cookie_names());
}

bool BoundSessionCookieControllerImpl::AreAllCookiesFresh() {
  return min_cookie_expiration_time() > base::Time::Now();
}

void BoundSessionCookieControllerImpl::MaybeRefreshCookie() {
  preemptive_cookie_refresh_timer_.Stop();
  if (refresh_cookie_fetcher_) {
    return;
  }

  if (!artifical_cookie_rotation_delay_) {
    StartCookieRefresh();
  } else if (!artifical_cookie_rotation_delay_->IsRunning()) {
    // Runs `StartCookieRefresh()` after a certain delay.
    artifical_cookie_rotation_delay_->Reset();
  }
}

void BoundSessionCookieControllerImpl::StartCookieRefresh() {
  CHECK(!refresh_cookie_fetcher_);

  if (artificial_cookie_rotation_result_) {
    OnCookieRefreshFetched(*artificial_cookie_rotation_result_);
  } else {
    refresh_cookie_fetcher_ = CreateRefreshCookieFetcher();
    // `base::Unretained(this)` is safe because `this` owns
    // `refresh_cookie_fetcher_`.
    refresh_cookie_fetcher_->Start(base::BindOnce(
        &BoundSessionCookieControllerImpl::OnCookieRefreshFetched,
        base::Unretained(this)));
  }
}

void BoundSessionCookieControllerImpl::OnCookieRefreshFetched(
    BoundSessionRefreshCookieFetcher::Result result) {
  UpdateDebugInfo(debug_info_, result,
                  refresh_cookie_fetcher_->IsChallengeReceived());
  refresh_cookie_fetcher_.reset();

  chrome::mojom::ResumeBlockedRequestsTrigger trigger =
      result == BoundSessionRefreshCookieFetcher::Result::kSuccess
          ? chrome::mojom::ResumeBlockedRequestsTrigger::
                kCookieRefreshFetchSuccess
          : chrome::mojom::ResumeBlockedRequestsTrigger::
                kCookieRefreshFetchFailure;
  // Resume blocked requests regardless of the result.
  // `SetCookieExpirationTimeAndNotify()` should be called around the same time
  // as `OnCookieRefreshFetched()` if the fetch was successful.
  ResumeBlockedRequests(trigger);

  // Persistent errors result in session termination.
  // Transient errors have no impact on future requests.

  if (BoundSessionRefreshCookieFetcher::IsPersistentError(result)) {
    delegate_->OnPersistentErrorEncountered(this);
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
    chrome::mojom::ResumeBlockedRequestsTrigger trigger) {
  resume_blocked_requests_timer_.Stop();
  if (resume_blocked_requests_.empty()) {
    return;
  }
  std::vector<chrome::mojom::BoundSessionRequestThrottledHandler::
                  HandleRequestBlockedOnCookieCallback>
      callbacks;
  std::swap(callbacks, resume_blocked_requests_);
  for (auto& callback : callbacks) {
    std::move(callback).Run(trigger);
  }
  base::UmaHistogramEnumeration(
      "Signin.BoundSessionCredentials.ResumeThrottledRequestsTrigger", trigger);
}

void BoundSessionCookieControllerImpl::OnResumeBlockedRequestsTimeout() {
  UpdateDebugInfo(debug_info_, TimeoutOccured{},
                  refresh_cookie_fetcher_->IsChallengeReceived());
  // Reset the fetcher, it has been taking at least
  // kResumeBlockedRequestTimeout. New requests will trigger a new fetch.
  refresh_cookie_fetcher_.reset();
  ResumeBlockedRequests(chrome::mojom::ResumeBlockedRequestsTrigger::kTimeout);
  successive_timeout_++;
}
