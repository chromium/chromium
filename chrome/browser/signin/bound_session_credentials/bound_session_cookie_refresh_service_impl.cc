// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_refresh_service_impl.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/barrier_callback.h"
#include "base/check.h"
#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_controller.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_controller_impl.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_debug_info.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_key.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_params_storage.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_params_util.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_refresh_cookie_debug_report_fetcher.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_registration_fetcher_impl.h"
#include "chrome/browser/signin/bound_session_credentials/rotation_debug_info.pb.h"
#include "chrome/common/google_url_loader_throttle.h"
#include "chrome/common/renderer_configuration.mojom.h"
#include "components/signin/public/base/signin_switches.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/schemeful_site.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/origin.h"

namespace {
const char kGoogleSessionTerminationHeader[] = "Sec-Session-Google-Termination";

// Determines the precedence order of
// `chrome::mojom::ResumeBlockedRequestsTrigger` when recording metrics.
size_t GetResumeBlockedRequestsTriggerPriority(
    chrome::mojom::ResumeBlockedRequestsTrigger trigger) {
  using chrome::mojom::ResumeBlockedRequestsTrigger;
  switch (trigger) {
    case ResumeBlockedRequestsTrigger::kCookieAlreadyFresh:
      return 0;
    case ResumeBlockedRequestsTrigger::kObservedFreshCookies:
      return 1;
    case ResumeBlockedRequestsTrigger::kCookieRefreshFetchSuccess:
      return 2;
    case ResumeBlockedRequestsTrigger::kShutdownOrSessionTermination:
      return 3;
    case ResumeBlockedRequestsTrigger::kRendererDisconnected:
      return 4;
    case ResumeBlockedRequestsTrigger::kCookieRefreshFetchFailure:
      return 5;
    case ResumeBlockedRequestsTrigger::kTimeout:
      return 6;
    case ResumeBlockedRequestsTrigger::kThrottlingRequestsPaused:
      return 7;
  }
}

// Computes a trigger value that should be used for metrics recording based on a
// list of triggers received from multiple controllers.
chrome::mojom::ResumeBlockedRequestsTrigger AggregateMultipleTriggers(
    std::vector<chrome::mojom::ResumeBlockedRequestsTrigger> triggers) {
  CHECK(!triggers.empty());
  return *std::max_element(
      triggers.begin(), triggers.end(),
      [](chrome::mojom::ResumeBlockedRequestsTrigger lhs,
         chrome::mojom::ResumeBlockedRequestsTrigger rhs) {
        return GetResumeBlockedRequestsTriggerPriority(lhs) <
               GetResumeBlockedRequestsTriggerPriority(rhs);
      });
}

bound_session_credentials::RotationDebugInfo::TerminationReason
GetRotationDebugTerminationReason(
    BoundSessionCookieRefreshServiceImpl::SessionTerminationTrigger trigger,
    std::optional<BoundSessionRefreshCookieFetcher::Result> refresh_error) {
  using enum BoundSessionCookieRefreshServiceImpl::SessionTerminationTrigger;
  using bound_session_credentials::RotationDebugInfo;

  auto GetReasonFromRefreshError =
      [](BoundSessionRefreshCookieFetcher::Result refresh_error) {
        using Result = BoundSessionRefreshCookieFetcher::Result;
        CHECK(
            BoundSessionRefreshCookieFetcher::IsPersistentError(refresh_error));
        switch (refresh_error) {
          case Result::kServerPersistentError:
            return RotationDebugInfo::ROTATION_PERSISTENT_ERROR;
          case Result::kServerUnexepectedResponse:
            return RotationDebugInfo::ROTATION_UNEXPECTED_RESPONSE;
          case Result::kChallengeRequiredUnexpectedFormat:
            return RotationDebugInfo::ROTATION_CHALLENGE_UNEXPECTED_FORMAT;
          case Result::kChallengeRequiredLimitExceeded:
            return RotationDebugInfo::ROTATION_CHALLENGE_LIMIT_EXCEEDED;
          case Result::kSignChallengeFailed:
            return RotationDebugInfo::ROTATION_SIGN_CHALLENGE_FAILED;
          default:
            return RotationDebugInfo::TERMINATION_REASON_OTHER;
        }
      };

  switch (trigger) {
    case kSessionTerminationHeader:
      return RotationDebugInfo::TERMINATION_HEADER_RECEIVED;
    case kCookieRotationPersistentError:
      return refresh_error ? GetReasonFromRefreshError(*refresh_error)
                           : RotationDebugInfo::TERMINATION_REASON_OTHER;
    case kSessionOverride:
      return RotationDebugInfo::SESSION_OVERRIDE;
    case kCookiesCleared:
      // `kCookiesCleared` should not be reported in the debug header.
      NOTREACHED();
  }
}

chrome::mojom::BoundSessionThrottlerParamsPtr
GetThrottlerParamsForRequestCoverage(
    const BoundSessionCookieController* controller) {
  if (!controller->ShouldPauseThrottlingRequests()) {
    return controller->bound_session_throttler_params();
  }

  // Throttling is paused, `chrome::mojom::BoundSessionThrottlerParamsPtr` is
  // expected to be null. Construct throttler params to compute request
  // coverage. Use time in the past to ensure that a throttler will be added to
  // blocking throttlers if it covers a request. The controller will resume the
  // request immediately.
  // Note: This is needed to ensure the correctness of metrics in case of
  // outages.
  return chrome::mojom::BoundSessionThrottlerParams::New(
      controller->scope_url().host(), controller->scope_url().path(),
      base::Time());
}
}  // namespace

BASE_FEATURE(kMultipleBoundSessionsEnabled,
             "MultipleBoundSessionsEnabled",
             base::FEATURE_ENABLED_BY_DEFAULT);

BoundSessionCookieRefreshServiceImpl::BoundSessionCookieRefreshServiceImpl(
    unexportable_keys::UnexportableKeyService& key_service,
    std::unique_ptr<BoundSessionParamsStorage> session_params_storage,
    content::StoragePartition* storage_partition,
    network::NetworkConnectionTracker* network_connection_tracker,
    bool is_off_the_record_profile)
    : key_service_(key_service),
      session_params_storage_(std::move(session_params_storage)),
      storage_partition_(storage_partition),
      network_connection_tracker_(network_connection_tracker),
      is_off_the_record_profile_(is_off_the_record_profile) {
  CHECK(session_params_storage_);
  CHECK(storage_partition_);
  data_removal_observation_.Observe(storage_partition_);
}

BoundSessionCookieRefreshServiceImpl::~BoundSessionCookieRefreshServiceImpl() =
    default;

void BoundSessionCookieRefreshServiceImpl::Initialize() {
  std::vector<bound_session_credentials::BoundSessionParams>
      bound_session_params =
          session_params_storage_->ReadAllParamsAndCleanStorageIfNecessary();
  if (bound_session_params.empty()) {
    return;
  }

  if (!base::FeatureList::IsEnabled(kMultipleBoundSessionsEnabled)) {
    InitializeBoundSession(bound_session_params.front());
    UpdateAllRenderers();
    return;
  }

  for (const auto& params : bound_session_params) {
    InitializeBoundSession(params);
  }
  UpdateAllRenderers();
}

void BoundSessionCookieRefreshServiceImpl::RegisterNewBoundSession(
    const bound_session_credentials::BoundSessionParams& params) {
  if (!session_params_storage_->SaveParams(params)) {
    DVLOG(1) << "Invalid session params or failed to serialize session params.";
    return;
  }

  if (base::FeatureList::IsEnabled(kMultipleBoundSessionsEnabled)) {
    // In the multi-session mode, we need to stop the controller corresponding
    // to the same session, if any.
    auto it = cookie_controllers_.find(
        bound_session_credentials::GetBoundSessionKey(params));
    if (it != cookie_controllers_.end()) {
      cookie_controllers_.erase(it);
      RecordSessionTerminationTrigger(
          SessionTerminationTrigger::kSessionOverride);
      // Note: `NotifyBoundSessionTerminated()` is not called as new session is
      // starting with the same scope.
    }
  } else {
    // In the single-session mode, we need to do the following:
    // - stop the current controller regardless of what session it controls, and
    // - clear storage entry for the current session if it doesn't match the new
    //   session.
    if (BoundSessionCookieController* controller = cookie_controller();
        controller) {
      bool clear_params = controller->GetBoundSessionKey() !=
                          bound_session_credentials::GetBoundSessionKey(params);
      if (clear_params) {
        session_params_storage_->ClearParams(controller->site(),
                                             controller->session_id());
      }
      cookie_controllers_.clear();
      // `controller` is no longer valid and must not be used.
      RecordSessionTerminationTrigger(
          SessionTerminationTrigger::kSessionOverride);
      // Note: `NotifyBoundSessionTerminated()` is not called as new session is
      // starting with the same scope.
    }
  }

  InitializeBoundSession(params);
  UpdateAllRenderers();
}

void BoundSessionCookieRefreshServiceImpl::MaybeTerminateSession(
    const GURL& response_url,
    const net::HttpResponseHeaders* headers) {
  if (!headers) {
    return;
  }

  std::string session_id;
  if (!headers->GetNormalizedHeader(kGoogleSessionTerminationHeader,
                                    &session_id)) {
    return;
  }

  BoundSessionKey key = {
      .site = net::SchemefulSite(response_url).GetURL(),
      .session_id = session_id,
  };
  auto it = cookie_controllers_.find(key);
  if (it != cookie_controllers_.end()) {
    TerminateSession(it->second.get(),
                     SessionTerminationTrigger::kSessionTerminationHeader);
  } else {
    DVLOG(1) << "Session termination header (" << key.site.spec() << "; "
             << key.session_id << ") doesn't match any current session";
  }
}

std::vector<chrome::mojom::BoundSessionThrottlerParamsPtr>
BoundSessionCookieRefreshServiceImpl::GetBoundSessionThrottlerParams() const {
  std::vector<chrome::mojom::BoundSessionThrottlerParamsPtr> result;
  for (const auto& [key, controller] : cookie_controllers_) {
    if (chrome::mojom::BoundSessionThrottlerParamsPtr params =
            controller->bound_session_throttler_params();
        params) {
      result.push_back(std::move(params));
    }
  }
  return result;
}

void BoundSessionCookieRefreshServiceImpl::
    SetRendererBoundSessionThrottlerParamsUpdaterDelegate(
        RendererBoundSessionThrottlerParamsUpdaterDelegate renderer_updater) {
  renderer_updater_ = std::move(renderer_updater);
}

void BoundSessionCookieRefreshServiceImpl::
    SetBoundSessionParamsUpdatedCallbackForTesting(
        base::RepeatingClosure updated_callback) {
  session_updated_callback_for_testing_ = std::move(updated_callback);
}

void BoundSessionCookieRefreshServiceImpl::
    AddBoundSessionRequestThrottledHandlerReceiver(
        mojo::PendingReceiver<
            chrome::mojom::BoundSessionRequestThrottledHandler> receiver) {
  renderer_request_throttled_handler_.Add(this, std::move(receiver));
}

void BoundSessionCookieRefreshServiceImpl::HandleRequestBlockedOnCookie(
    const GURL& untrusted_request_url,
    HandleRequestBlockedOnCookieCallback resume_blocked_request) {
  if (cookie_controllers_.empty()) {
    // Session has been terminated.
    std::move(resume_blocked_request)
        .Run(chrome::mojom::ResumeBlockedRequestsTrigger::
                 kShutdownOrSessionTermination);
    return;
  }

  std::vector<BoundSessionCookieController*> blocking_controllers;
  bool request_covered_by_at_least_one_session = false;
  if (!base::FeatureList::IsEnabled(kMultipleBoundSessionsEnabled)) {
    blocking_controllers.push_back(cookie_controller());
    // Assume by default that the only controller covers all incoming
    // requests.
    request_covered_by_at_least_one_session = true;
  } else {
    for (const auto& [key, controller] : cookie_controllers_) {
      std::vector<chrome::mojom::BoundSessionThrottlerParamsPtr>
          throttler_params;
      throttler_params.push_back(
          GetThrottlerParamsForRequestCoverage(controller.get()));
      GoogleURLLoaderThrottle::RequestBoundSessionStatus status =
          GoogleURLLoaderThrottle::GetRequestBoundSessionStatus(
              untrusted_request_url, throttler_params);
      switch (status) {
        case GoogleURLLoaderThrottle::RequestBoundSessionStatus::
            kCoveredWithMissingCookie:
          blocking_controllers.push_back(controller.get());
          [[fallthrough]];
        case GoogleURLLoaderThrottle::RequestBoundSessionStatus::
            kCoveredWithFreshCookie:
          request_covered_by_at_least_one_session = true;
          break;
        case GoogleURLLoaderThrottle::RequestBoundSessionStatus::kNotCovered:
          break;
      }
    }
  }

  if (blocking_controllers.empty()) {
    std::move(resume_blocked_request)
        .Run(request_covered_by_at_least_one_session
                 ? chrome::mojom::ResumeBlockedRequestsTrigger::
                       kCookieAlreadyFresh
                 : chrome::mojom::ResumeBlockedRequestsTrigger::
                       kShutdownOrSessionTermination);
    return;
  }

  base::RepeatingCallback<void(chrome::mojom::ResumeBlockedRequestsTrigger)>
      barrier_callback =
          base::BarrierCallback<chrome::mojom::ResumeBlockedRequestsTrigger>(
              blocking_controllers.size(),
              base::BindOnce(&AggregateMultipleTriggers)
                  .Then(std::move(resume_blocked_request)));
  for (BoundSessionCookieController* controller : blocking_controllers) {
    controller->HandleRequestBlockedOnCookie(barrier_callback);
  }
}

void BoundSessionCookieRefreshServiceImpl::CreateRegistrationRequest(
    BoundSessionRegistrationFetcherParam registration_params) {
  // Guardrail against registering non-SIDTS DBSC sessions while the client
  // lacks support for running multiple sessions at the same time. Can be
  // overridden with a Finch config parameter.
  // TODO(http://b/274774185): Remove this guardrail once ready.
  std::string exclusive_registration_path =
      switches::kEnableBoundSessionCredentialsExclusiveRegistrationPath.Get();
  if (!exclusive_registration_path.empty() &&
      !base::EqualsCaseInsensitiveASCII(
          registration_params.registration_endpoint().path_piece(),
          exclusive_registration_path)) {
    return;
  }

  if (!registration_requests_.empty() &&
      !base::FeatureList::IsEnabled(kMultipleBoundSessionsEnabled)) {
    // If there are multiple racing registration requests, only one will be
    // processed and it will contain the most up-to-date set of cookies.
    return;
  }

  registration_requests_.push(
      registration_fetcher_factory_for_testing_.is_null()
          ? std::make_unique<BoundSessionRegistrationFetcherImpl>(
                std::move(registration_params),
                storage_partition_->GetURLLoaderFactoryForBrowserProcess(),
                key_service_.get(), is_off_the_record_profile_)
          : registration_fetcher_factory_for_testing_.Run(
                std::move(registration_params)));

  if (registration_requests_.size() == 1U) {
    StartRegistrationRequest();
  }
}

base::WeakPtr<BoundSessionCookieRefreshService>
BoundSessionCookieRefreshServiceImpl::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void BoundSessionCookieRefreshServiceImpl::AddObserver(
    BoundSessionCookieRefreshService::Observer* observer) {
  observers_.AddObserver(observer);
}

void BoundSessionCookieRefreshServiceImpl::RemoveObserver(
    BoundSessionCookieRefreshService::Observer* observer) {
  observers_.RemoveObserver(observer);
}

std::vector<BoundSessionDebugInfo>
BoundSessionCookieRefreshServiceImpl::GetBoundSessionDebugInfo() const {
  std::vector<BoundSessionDebugInfo> bound_session_debug_info;
  for (const auto& [key, controller] : cookie_controllers_) {
    bound_session_debug_info.push_back(
        BoundSessionDebugInfo::Create(*controller));
  }
  return bound_session_debug_info;
}

BoundSessionCookieController*
BoundSessionCookieRefreshServiceImpl::cookie_controller() const {
  if (cookie_controllers_.empty()) {
    return nullptr;
  }
  return cookie_controllers_.begin()->second.get();
}

void BoundSessionCookieRefreshServiceImpl::StartRegistrationRequest() {
  // `base::Unretained(this)` is safe here because `this` owns the fetcher via
  // `registration_requests_`
  registration_requests_.front()->Start(base::BindOnce(
      &BoundSessionCookieRefreshServiceImpl::OnRegistrationRequestComplete,
      base::Unretained(this)));
}

void BoundSessionCookieRefreshServiceImpl::OnRegistrationRequestComplete(
    std::optional<bound_session_credentials::BoundSessionParams>
        bound_session_params) {
  if (bound_session_params.has_value()) {
    RegisterNewBoundSession(*bound_session_params);
  }

  registration_requests_.pop();
  if (!registration_requests_.empty()) {
    StartRegistrationRequest();
  }
}

void BoundSessionCookieRefreshServiceImpl::
    OnBoundSessionThrottlerParamsChanged() {
  UpdateAllRenderers();
}

void BoundSessionCookieRefreshServiceImpl::OnPersistentErrorEncountered(
    BoundSessionCookieController* controller,
    BoundSessionRefreshCookieFetcher::Result refresh_error) {
  TerminateSession(controller,
                   SessionTerminationTrigger::kCookieRotationPersistentError,
                   refresh_error);
}

void BoundSessionCookieRefreshServiceImpl::OnStorageKeyDataCleared(
    uint32_t remove_mask,
    content::StoragePartition::StorageKeyMatcherFunction storage_key_matcher,
    const base::Time begin,
    const base::Time end) {
  // Only terminate a session if cookies are cleared.
  // TODO(b/296372836): introduce a specific data type for bound sessions.
  if (!(remove_mask & content::StoragePartition::REMOVE_DATA_MASK_COOKIES)) {
    return;
  }

  std::vector<BoundSessionCookieController*> controllers_to_remove;
  for (auto& [key, controller] : cookie_controllers_) {
    // Only terminate a session if it was created within the specified time
    // range.
    base::Time session_creation_time = controller->session_creation_time();
    if (session_creation_time < begin || session_creation_time > end) {
      continue;
    }

    // Only terminate a session if its URL matches `storage_key_matcher`.
    // Bound sessions are only supported in first-party contexts, so it's
    // acceptable to use `blink::StorageKey::CreateFirstParty()`.
    if (!storage_key_matcher.Run(blink::StorageKey::CreateFirstParty(
            url::Origin::Create(controller->scope_url())))) {
      continue;
    }

    // `TerminateSession()` cannot be called while iterating over
    // `cookie_controllers_`.
    controllers_to_remove.push_back(controller.get());
  }

  for (const auto& controller : controllers_to_remove) {
    TerminateSession(controller, SessionTerminationTrigger::kCookiesCleared);
  }
}

std::unique_ptr<BoundSessionCookieController>
BoundSessionCookieRefreshServiceImpl::CreateBoundSessionCookieController(
    const bound_session_credentials::BoundSessionParams& bound_session_params,
    bool is_off_the_record_profile) {
  return controller_factory_for_testing_.is_null()
             ? std::make_unique<BoundSessionCookieControllerImpl>(
                   key_service_.get(), storage_partition_,
                   network_connection_tracker_, bound_session_params, this,
                   is_off_the_record_profile)
             : controller_factory_for_testing_.Run(bound_session_params, this);
}

void BoundSessionCookieRefreshServiceImpl::InitializeBoundSession(
    const bound_session_credentials::BoundSessionParams& bound_session_params) {
  if (!base::FeatureList::IsEnabled(kMultipleBoundSessionsEnabled)) {
    CHECK(cookie_controllers_.empty());
  }
  std::unique_ptr<BoundSessionCookieController> controller =
      CreateBoundSessionCookieController(bound_session_params,
                                         is_off_the_record_profile_);
  BoundSessionKey key = controller->GetBoundSessionKey();
  auto [it, inserted] =
      cookie_controllers_.emplace(std::move(key), std::move(controller));
  CHECK(inserted);
  it->second->Initialize();
}

void BoundSessionCookieRefreshServiceImpl::UpdateAllRenderers() {
  if (renderer_updater_) {
    renderer_updater_.Run();
  }
  if (session_updated_callback_for_testing_) {
    session_updated_callback_for_testing_.Run();
  }
}

void BoundSessionCookieRefreshServiceImpl::TerminateSession(
    BoundSessionCookieController* controller,
    SessionTerminationTrigger trigger,
    std::optional<BoundSessionRefreshCookieFetcher::Result> refresh_error) {
  BoundSessionKey session_key = controller->GetBoundSessionKey();
  auto it = cookie_controllers_.find(session_key);
  CHECK(it != cookie_controllers_.end());
  CHECK_EQ(it->second.get(), controller);
  // Save `bound_cookie_names` locally on the stack before destroying
  // `controller`.
  base::flat_set<std::string> bound_cookie_names =
      controller->bound_cookie_names();
  MaybeReportTerminationReason(controller, trigger, refresh_error);
  cookie_controllers_.erase(it);
  // `controller` is no longer valid and must not be used.

  session_params_storage_->ClearParams(session_key.site,
                                       session_key.session_id);
  UpdateAllRenderers();
  RecordSessionTerminationTrigger(trigger);

  NotifyBoundSessionTerminated(session_key.site, bound_cookie_names);
}

void BoundSessionCookieRefreshServiceImpl::RecordSessionTerminationTrigger(
    SessionTerminationTrigger trigger) {
  base::UmaHistogramEnumeration(
      "Signin.BoundSessionCredentials.SessionTerminationTrigger", trigger);
}

void BoundSessionCookieRefreshServiceImpl::NotifyBoundSessionTerminated(
    const GURL& site,
    const base::flat_set<std::string>& bound_cookie_names) {
  for (BoundSessionCookieRefreshService::Observer& observer : observers_) {
    observer.OnBoundSessionTerminated(site, bound_cookie_names);
  }
}

void BoundSessionCookieRefreshServiceImpl::MaybeReportTerminationReason(
    BoundSessionCookieController* controller,
    SessionTerminationTrigger trigger,
    std::optional<BoundSessionRefreshCookieFetcher::Result> refresh_error) {
  if (trigger == SessionTerminationTrigger::kCookiesCleared) {
    // Do not send the debug report if cookies were cleared as the request won't
    // be attributed to a user in any case.
    return;
  }

  bound_session_credentials::RotationDebugInfo debug_info =
      controller->TakeDebugInfo();
  debug_info.set_termination_reason(
      GetRotationDebugTerminationReason(trigger, refresh_error));
  auto reporter =
      debug_report_fetcher_factory_for_testing_.is_null()
          ? std::make_unique<BoundSessionRefreshCookieDebugReportFetcher>(
                storage_partition_->GetURLLoaderFactoryForBrowserProcess(),
                controller->session_id(), controller->refresh_url(),
                is_off_the_record_profile_, std::move(debug_info))
          : debug_report_fetcher_factory_for_testing_.Run(
                controller->session_id(), controller->refresh_url(),
                is_off_the_record_profile_, std::move(debug_info));
  CHECK(reporter);
  BoundSessionRefreshCookieFetcher* reporter_raw = reporter.get();
  termination_reason_reporters_.insert(std::move(reporter));
  // `base::Unretained()` is safe because `this` owns
  // `termination_reason_reporters_`.
  reporter_raw->Start(base::BindOnce(&BoundSessionCookieRefreshServiceImpl::
                                         OnTerminationReasonReportCompleted,
                                     base::Unretained(this), reporter_raw),
                      "SESSION_TERMINATION_DEBUG_REPORT");
}

void BoundSessionCookieRefreshServiceImpl::OnTerminationReasonReportCompleted(
    BoundSessionRefreshCookieFetcher* reporter,
    BoundSessionRefreshCookieFetcher::Result result) {
  auto it = termination_reason_reporters_.find(reporter);
  CHECK(it != termination_reason_reporters_.end());
  termination_reason_reporters_.erase(it);
}
