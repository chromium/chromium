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
#include "net/base/backoff_entry.h"
#include "services/network/public/cpp/network_connection_tracker.h"

namespace {
using Result = BoundSessionRefreshCookieFetcher::Result;
using ResumeBlockedRequestsTrigger =
    chrome::mojom::ResumeBlockedRequestsTrigger;

constexpr size_t kMaxRetrialsForThrottledRequestsOnTransientError = 1u;
constexpr int kNumberOfErrorsToIgnoreForBackoff = 3;

constexpr net::BackoffEntry::Policy kBackoffPolicy = {
    // Number of initial errors (in sequence) to ignore before applying
    // exponential back-off rules.
    kNumberOfErrorsToIgnoreForBackoff,

    // Initial delay for exponential backoff in ms.
    500,

    // Factor by which the waiting time will be multiplied.
    1.5,

    // Fuzzing percentage. ex: 10% will spread requests randomly
    // between 90%-100% of the calculated time.
    0.2,  // 20%

    // Maximum amount of time we are willing to delay our request in ms.
    1000 * 60 * 8,  // 8 Minutes

    // Time to keep an entry from being discarded even when it
    // has no significant state, -1 to never discard.
    -1,

    // Don't use initial delay unless the last request was an error.
    false,
};

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
      is_off_the_record_profile_(is_off_the_record_profile),
      refresh_cookie_fetcher_backoff_(&kBackoffPolicy) {
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
  RecordNumberOfSuccessiveTimeoutIfAny(successive_timeout_);
  RecordCookieRotationOutageMetricsIfNeeded(/*periodic=*/false);
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
        .Run(ResumeBlockedRequestsTrigger::kCookieAlreadyFresh);
    return;
  }

  if (ShouldPauseThrottlingRequests()) {
    std::move(resume_blocked_request)
        .Run(ResumeBlockedRequestsTrigger::kThrottlingRequestsPaused);
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

bool BoundSessionCookieControllerImpl::ShouldPauseThrottlingRequests() const {
  return refresh_cookie_fetcher_backoff_.failure_count() >
         kNumberOfErrorsToIgnoreForBackoff;
}

bound_session_credentials::RotationDebugInfo
BoundSessionCookieControllerImpl::TakeDebugInfo() {
  bound_session_credentials::RotationDebugInfo return_info;
  // Leave `debug_info_` in a specified empty state.
  return_info.Swap(&debug_info_);
  return return_info;
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
    ResetCookieFetcherBackoff();
    ResumeBlockedRequests(ResumeBlockedRequestsTrigger::kObservedFreshCookies);
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
            storage_partition_, scope_url_, cookie_name,
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
                   *session_binding_helper_, session_id_, refresh_url_,
                   scope_url_, bound_cookie_names(), is_off_the_record_profile_,
                   debug_info_)
             : refresh_cookie_fetcher_factory_for_testing_.Run(
                   storage_partition_->GetCookieManagerForBrowserProcess(),
                   scope_url_, bound_cookie_names());
}

bool BoundSessionCookieControllerImpl::AreAllCookiesFresh() {
  return min_cookie_expiration_time() > base::Time::Now();
}

bool BoundSessionCookieControllerImpl::CanCreateRefreshCookieFetcher() const {
  return !refresh_cookie_fetcher_ &&
         !refresh_cookie_fetcher_backoff_.ShouldRejectRequest();
}

void BoundSessionCookieControllerImpl::MaybeRefreshCookie() {
  cookie_refresh_timer_.Stop();
  if (!CanCreateRefreshCookieFetcher()) {
    return;
  }

  refresh_cookie_fetcher_ = CreateRefreshCookieFetcher();
  std::optional<std::string> sec_session_challenge_response;
  std::swap(cached_sec_session_challenge_response_,
            sec_session_challenge_response);
  // `base::Unretained(this)` is safe because `this` owns
  // `refresh_cookie_fetcher_`.
  refresh_cookie_fetcher_->Start(
      base::BindOnce(&BoundSessionCookieControllerImpl::OnCookieRefreshFetched,
                     base::Unretained(this)),
      std::move(sec_session_challenge_response));
}

void BoundSessionCookieControllerImpl::OnCookieRefreshFetched(Result result) {
  CHECK(!cached_sec_session_challenge_response_.has_value());
  bool is_transient_error =
      BoundSessionRefreshCookieFetcher::IsTransientError(result);
  if (is_transient_error) {
    // In case of server transient error or network error, try to reuse the
    // challenge response if there is one on the next cookie rotation request.
    // Note: The challenge might be expired by the time the cached response is
    // used, it is expected in that case that the server would request a new
    // challenge response.
    cached_sec_session_challenge_response_ =
        refresh_cookie_fetcher_->TakeSecSessionChallengeResponseIfAny();
  }

  UpdateDebugInfo(debug_info_, result,
                  refresh_cookie_fetcher_->IsChallengeReceived());
  refresh_cookie_fetcher_.reset();

  UpdateCookieFetcherBackoff(result);
  if (CanCreateRefreshCookieFetcher() &&
      cookie_rotation_retries_on_transient_error_ <
          kMaxRetrialsForThrottledRequestsOnTransientError &&
      is_transient_error && !resume_blocked_requests_.empty()) {
    // On transient error, retry the cookie refresh before releasing
    // throttled requests. Do not retry preemptive refreshes.
    cookie_rotation_retries_on_transient_error_++;
    MaybeRefreshCookie();
    return;
  }

  // Resume blocked requests regardless of the result.
  // `SetCookieExpirationTimeAndNotify()` should be called around the same time
  // as `OnCookieRefreshFetched()` if the fetch was successful.

  if (result == BoundSessionRefreshCookieFetcher::Result::kSuccess) {
    ResumeBlockedRequests(
        ResumeBlockedRequestsTrigger::kCookieRefreshFetchSuccess);
    cookie_rotation_retries_on_transient_error_ = 0;
    return;
  }

  ResumeBlockedRequests(
      ResumeBlockedRequestsTrigger::kCookieRefreshFetchFailure);

  // Persistent errors result in session termination.
  // Transient errors have no impact on future requests.
  if (BoundSessionRefreshCookieFetcher::IsPersistentError(result)) {
    delegate_->OnPersistentErrorEncountered(this, result);
    // `this` should be deleted.
  }
}

void BoundSessionCookieControllerImpl::UpdateCookieFetcherBackoff(
    Result result) {
  if (result == Result::kSuccess) {
    ResetCookieFetcherBackoff();
    return;
  }

  bool was_throttling_requests_paused = ShouldPauseThrottlingRequests();
  if (!was_throttling_requests_paused &&
      result != Result::kServerTransientError) {
    return;
  }

  // If throttling is paused, ensure that other types of errors:
  // - Do not stop cookie rotation in the background
  // - Do not retry in a loop but with backoff
  refresh_cookie_fetcher_backoff_.InformOfRequest(/*succeeded=*/false);
  if (!ShouldPauseThrottlingRequests()) {
    return;
  }

  if (!was_throttling_requests_paused) {
    // Notify only once.
    CHECK(!cookie_rotation_outage_start_.has_value());
    cookie_rotation_outage_start_ = base::TimeTicks::Now();
    delegate_->OnBoundSessionThrottlerParamsChanged();
  }
  RecordCookieRotationOutageMetricsIfNeeded(/*periodic=*/true);

  // Request throttling is paused due to an outage, schedule cookie rotation in
  // the background with backoff.
  MaybeScheduleCookieRotation();
}

void BoundSessionCookieControllerImpl::
    RecordCookieRotationOutageMetricsIfNeeded(bool periodic) {
  if (!ShouldPauseThrottlingRequests()) {
    // No ongoing outage.
    return;
  }

  // Recorded periodically every `kNumberOfErrorsToIgnoreForBackoff + 1`
  // failures during an outage.
  static const size_t kPeriodicInterval = kNumberOfErrorsToIgnoreForBackoff + 1;
  if (periodic) {
    if (refresh_cookie_fetcher_backoff_.failure_count() % kPeriodicInterval ==
        0) {
      base::UmaHistogramCounts100(
          "Signin.BoundSessionCredentials.CookieRotationOutageAttemptsPeriodic",
          refresh_cookie_fetcher_backoff_.failure_count());
    }
    return;
  }

  // Note: If an outage is terminated as a result of a successful cookie
  // rotation, it won't be counted as we only count failures.
  base::UmaHistogramCounts100(
      "Signin.BoundSessionCredentials.CookieRotationOutageAttempts",
      refresh_cookie_fetcher_backoff_.failure_count());
  CHECK(cookie_rotation_outage_start_.has_value());
  base::TimeDelta duration =
      base::TimeTicks::Now() - *cookie_rotation_outage_start_;
  cookie_rotation_outage_start_.reset();
  base::UmaHistogramMediumTimes(
      "Signin.BoundSessionCredentials.CookieRotationOutageDuration", duration);
}

void BoundSessionCookieControllerImpl::ResetCookieFetcherBackoff() {
  RecordCookieRotationOutageMetricsIfNeeded(/*periodic=*/false);
  refresh_cookie_fetcher_backoff_.Reset();
  // Note: It is expected that required cookies must become fresh with
  // successful cookie rotation. Cookie rotation request that does not set
  // required cookies is considered as a persistent failure. We rely on cookie
  // updates to trigger `delegate->OnBoundSessionThrottlerParamsChanged()`.
  // Cookie rotation is also expected to be scheduled based on newly rotated
  // cookie's expiration date.
}

void BoundSessionCookieControllerImpl::MaybeScheduleCookieRotation() {
  const base::TimeDelta kCookieRefreshInterval = base::Minutes(2);
  base::TimeDelta preemptive_refresh_in =
      min_cookie_expiration_time() - base::Time::Now() - kCookieRefreshInterval;

  // Respect backoff release time if set, otherwise follow the time for
  // preemptive cookie refresh.
  base::TimeDelta refresh_in =
      ShouldPauseThrottlingRequests()
          ? refresh_cookie_fetcher_backoff_.GetTimeUntilRelease()
          : preemptive_refresh_in;

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

void BoundSessionCookieControllerImpl::ResumeBlockedRequests(
    ResumeBlockedRequestsTrigger trigger) {
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
  ResumeBlockedRequests(ResumeBlockedRequestsTrigger::kTimeout);
  successive_timeout_++;
}
