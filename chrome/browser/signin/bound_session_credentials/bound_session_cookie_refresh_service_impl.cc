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
  // Only one session is supported at the moment. New session should override an
  // existing one.
  // TODO(http://b/325451275): only remove a session that has a matching key.
  if (BoundSessionCookieController* controller = cookie_controller();
      controller) {
    bool clear_params = controller->url().spec() != params.site() ||
                        controller->session_id() != params.session_id();
    if (clear_params) {
      session_params_storage_->ClearParams(controller->url().spec(),
                                           controller->session_id());
    }
    cookie_controllers_.clear();
    // `controller` is no longer valid and must not be used.
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
  if (!headers->GetNormalizedHeader(kGoogleSessionTerminationHeader,
                                    &session_id)) {
    return;
  }

  if (BoundSessionCookieController* controller = cookie_controller();
      controller && controller->session_id() == session_id) {
    TerminateSession(SessionTerminationTrigger::kSessionTerminationHeader);
  } else {
    DVLOG(1) << "Session termination header (session_id=" << session_id
             << ") doesn't match any current session";
  }
}

chrome::mojom::BoundSessionThrottlerParamsPtr
BoundSessionCookieRefreshServiceImpl::GetBoundSessionThrottlerParams() const {
  if (cookie_controllers_.empty()) {
    return chrome::mojom::BoundSessionThrottlerParamsPtr();
  }

  // TODO(http://b/325451269): update BoundSessionThrottlerParams to include
  // include a list of bound sessions.
  return cookie_controller()->bound_session_throttler_params();
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
  if (cookie_controllers_.empty()) {
    // Session has been terminated.
    std::move(resume_blocked_request)
        .Run(chrome::mojom::ResumeBlockedRequestsTrigger::
                 kShutdownOrSessionTermination);
    return;
  }
  // TODO(http://b/325450696): propagate the callback to all relevant
  // controllers.
  cookie_controller()->HandleRequestBlockedOnCookie(
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
          registration_params.registration_endpoint().path_piece(),
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

BoundSessionCookieController*
BoundSessionCookieRefreshServiceImpl::cookie_controller() const {
  if (cookie_controllers_.empty()) {
    return nullptr;
  }
  return cookie_controllers_.begin()->second.get();
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
  BoundSessionCookieController* controller = cookie_controller();
  // No active session is running. Nothing to terminate.
  if (!controller) {
    return;
  }

  // Only terminate a session if cookies are cleared.
  // TODO(b/296372836): introduce a specific data type for bound sessions.
  if (!(remove_mask & content::StoragePartition::REMOVE_DATA_MASK_COOKIES)) {
    return;
  }

  // Only terminate a session if it was created within the specified time range.
  base::Time session_creation_time = controller->session_creation_time();
  if (session_creation_time < begin || session_creation_time > end) {
    return;
  }

  // Only terminate a session if its URL matches `storage_key_matcher`.
  // Bound sessions are only supported in first-party contexts, so it's
  // acceptable to use `blink::StorageKey::CreateFirstParty()`.
  if (!storage_key_matcher.Run(blink::StorageKey::CreateFirstParty(
          url::Origin::Create(controller->url())))) {
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
  // TODO(http://b/325451275): remove this CHECK once multiple sessions are
  // supported.
  CHECK(cookie_controllers_.empty());
  std::unique_ptr<BoundSessionCookieController> controller =
      CreateBoundSessionCookieController(bound_session_params,
                                         is_off_the_record_profile_);
  BoundSessionKey key = {.site = controller->url(),
                         .session_id = controller->session_id()};
  auto [it, inserted] =
      cookie_controllers_.emplace(std::move(key), std::move(controller));
  CHECK(inserted);
  it->second->Initialize();
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
  BoundSessionCookieController* controller = cookie_controller();
  CHECK(controller);
  // Save some values locally on the stack before destroying `controller`.
  GURL session_url = controller->url();
  base::flat_set<std::string> bound_cookie_names =
      controller->bound_cookie_names();
  cookie_controllers_.clear();
  // `controller` is no longer valid and must not be used.

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
