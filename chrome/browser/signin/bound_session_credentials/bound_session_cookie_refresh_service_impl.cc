// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_refresh_service_impl.h"

#include <memory>
#include <optional>

#include "base/check.h"
#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_controller.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_controller_impl.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_params_storage.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_registration_fetcher_impl.h"
#include "chrome/common/renderer_configuration.mojom.h"
#include "components/signin/public/base/signin_switches.h"
#include "content/public/browser/storage_partition.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/origin.h"

namespace {
const char kGoogleSessionTerminationHeader[] = "Sec-Session-Google-Termination";
}  // namespace

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
      bound_session_params = session_params_storage_->ReadAllParams();
  if (!bound_session_params.empty()) {
    // Only a single bound session is currently supported.
    // TODO(http://b/274774185): support multiple parallel bound sessions.
    InitializeBoundSession(bound_session_params.front());
  }
}

void BoundSessionCookieRefreshServiceImpl::RegisterNewBoundSession(
    const bound_session_credentials::BoundSessionParams& params) {
  if (!session_params_storage_->SaveParams(params)) {
    DVLOG(1) << "Invalid session params or failed to serialize session params.";
    return;
  }
  // New session should override an existing one.
  if (cookie_controller_) {
    bool clear_params = cookie_controller_->url().spec() != params.site() ||
                        cookie_controller_->session_id() != params.session_id();
    if (clear_params) {
      session_params_storage_->ClearParams(cookie_controller_->url().spec(),
                                           cookie_controller_->session_id());
    }
    cookie_controller_.reset();
    RecordSessionTerminationTrigger(
        SessionTerminationTrigger::kSessionOverride);
    // Note: `NotifyBoundSessionTerminated()` is not called as new session is
    // starting with the same scope.
  }
  InitializeBoundSession(params);
}

void BoundSessionCookieRefreshServiceImpl::MaybeTerminateSession(
    const net::HttpResponseHeaders* headers) {
  if (!headers) {
    return;
  }

  std::string session_id;
  if (headers->GetNormalizedHeader(kGoogleSessionTerminationHeader,
                                   &session_id)) {
    if (session_id == cookie_controller_->session_id()) {
      TerminateSession(SessionTerminationTrigger::kSessionTerminationHeader);
    } else {
      DVLOG(1) << "Session id on session termination header (" << session_id
               << ") doesn't match with the current session id ("
               << cookie_controller_->session_id() << ")";
    }
  }
}

chrome::mojom::BoundSessionThrottlerParamsPtr
BoundSessionCookieRefreshServiceImpl::GetBoundSessionThrottlerParams() const {
  if (!cookie_controller_) {
    return chrome::mojom::BoundSessionThrottlerParamsPtr();
  }

  return cookie_controller_->bound_session_throttler_params();
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
    HandleRequestBlockedOnCookieCallback resume_blocked_request) {
  if (!cookie_controller_) {
    // Session has been terminated.
    std::move(resume_blocked_request)
        .Run(chrome::mojom::ResumeBlockedRequestsTrigger::
                 kShutdownOrSessionTermination);
    return;
  }
  cookie_controller_->HandleRequestBlockedOnCookie(
      std::move(resume_blocked_request));
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
          registration_params.RegistrationEndpoint().path_piece(),
          exclusive_registration_path)) {
    return;
  }

  if (active_registration_request_) {
    // If there are multiple racing registration requests, only one will be
    // processed and it will contain the most up-to-date set of cookies.
    return;
  }

  active_registration_request_ =
      registration_fetcher_factory_for_testing_.is_null()
          ? std::make_unique<BoundSessionRegistrationFetcherImpl>(
                std::move(registration_params),
                storage_partition_->GetURLLoaderFactoryForBrowserProcess(),
                key_service_.get(), is_off_the_record_profile_)
          : registration_fetcher_factory_for_testing_.Run(
                std::move(registration_params));
  // `base::Unretained(this)` is safe here because `this` owns the fetcher via
  // `active_registration_requests_`
  active_registration_request_->Start(base::BindOnce(
      &BoundSessionCookieRefreshServiceImpl::OnRegistrationRequestComplete,
      base::Unretained(this)));
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

void BoundSessionCookieRefreshServiceImpl::OnRegistrationRequestComplete(
    std::optional<bound_session_credentials::BoundSessionParams>
        bound_session_params) {
  if (bound_session_params.has_value()) {
    RegisterNewBoundSession(*bound_session_params);
  }

  active_registration_request_.reset();
}

void BoundSessionCookieRefreshServiceImpl::
    OnBoundSessionThrottlerParamsChanged() {
  UpdateAllRenderers();
}

void BoundSessionCookieRefreshServiceImpl::OnPersistentErrorEncountered() {
  TerminateSession(SessionTerminationTrigger::kCookieRotationPersistentError);
}

void BoundSessionCookieRefreshServiceImpl::OnStorageKeyDataCleared(
    uint32_t remove_mask,
    content::StoragePartition::StorageKeyMatcherFunction storage_key_matcher,
    const base::Time begin,
    const base::Time end) {
  // No active session is running. Nothing to terminate.
  if (!cookie_controller_) {
    return;
  }

  // Only terminate a session if cookies are cleared.
  // TODO(b/296372836): introduce a specific data type for bound sessions.
  if (!(remove_mask & content::StoragePartition::REMOVE_DATA_MASK_COOKIES)) {
    return;
  }

  // Only terminate a session if it was created within the specified time range.
  base::Time session_creation_time =
      cookie_controller_->session_creation_time();
  if (session_creation_time < begin || session_creation_time > end) {
    return;
  }

  // Only terminate a session if its URL matches `storage_key_matcher`.
  // Bound sessions are only supported in first-party contexts, so it's
  // acceptable to use `blink::StorageKey::CreateFirstParty()`.
  if (!storage_key_matcher.Run(blink::StorageKey::CreateFirstParty(
          url::Origin::Create(cookie_controller_->url())))) {
    return;
  }

  TerminateSession(SessionTerminationTrigger::kCookiesCleared);
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
  CHECK(!cookie_controller_);
  cookie_controller_ = CreateBoundSessionCookieController(
      bound_session_params, is_off_the_record_profile_);
  cookie_controller_->Initialize();
  UpdateAllRenderers();
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
    SessionTerminationTrigger trigger) {
  CHECK(cookie_controller_);
  GURL session_url = cookie_controller_->url();
  base::flat_set<std::string> bound_cookie_names =
      cookie_controller_->bound_cookie_names();
  cookie_controller_.reset();
  // TODO(b/300627729): stop clearing all params once multiple sessions are
  // supported.
  session_params_storage_->ClearAllParams();
  UpdateAllRenderers();
  RecordSessionTerminationTrigger(trigger);

  NotifyBoundSessionTerminated(session_url, bound_cookie_names);
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
