// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_refresh_service_impl.h"

#include <memory>

#include "base/base64.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_controller.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_controller_impl.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_params_storage.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_registration_fetcher_impl.h"
#include "chrome/common/renderer_configuration.mojom.h"
#include "components/signin/public/base/signin_client.h"
#include "content/public/browser/storage_partition.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace {
const char kGoogleSessionTerminationHeader[] = "Sec-Session-Google-Termination";
}

BoundSessionCookieRefreshServiceImpl::BoundSessionCookieRefreshServiceImpl(
    unexportable_keys::UnexportableKeyService& key_service,
    std::unique_ptr<BoundSessionParamsStorage> session_params_storage,
    content::StoragePartition* storage_partition,
    network::NetworkConnectionTracker* network_connection_tracker)
    : key_service_(key_service),
      session_params_storage_(std::move(session_params_storage)),
      storage_partition_(storage_partition),
      network_connection_tracker_(network_connection_tracker) {
  CHECK(session_params_storage_);
}

BoundSessionCookieRefreshServiceImpl::~BoundSessionCookieRefreshServiceImpl() =
    default;

void BoundSessionCookieRefreshServiceImpl::Initialize() {
  absl::optional<bound_session_credentials::RegistrationParams>
      registration_params = session_params_storage_->ReadParams();
  if (registration_params.has_value()) {
    InitializeBoundSession(registration_params.value());
  }
}

void BoundSessionCookieRefreshServiceImpl::RegisterNewBoundSession(
    const bound_session_credentials::RegistrationParams& params) {
  if (!session_params_storage_->SaveParams(params)) {
    DVLOG(1) << "Invalid session params or failed to serialize bound session "
                "registration params.";
    return;
  }
  // New session should override an existing one.
  cookie_controller_.reset();
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
    // TODO(b/293433229): Verify `session_id` matches the current session's id.
    TerminateSession();
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
  renderer_updater_ = renderer_updater;
}

void BoundSessionCookieRefreshServiceImpl::
    AddBoundSessionRequestThrottledListenerReceiver(
        mojo::PendingReceiver<
            chrome::mojom::BoundSessionRequestThrottledListener> receiver) {
  renderer_request_throttled_listener_.Add(this, std::move(receiver));
}

void BoundSessionCookieRefreshServiceImpl::OnRequestBlockedOnCookie(
    OnRequestBlockedOnCookieCallback resume_blocked_request) {
  if (!cookie_controller_) {
    // Session has been terminated.
    std::move(resume_blocked_request).Run();
    return;
  }
  cookie_controller_->OnRequestBlockedOnCookie(
      std::move(resume_blocked_request));
}

void BoundSessionCookieRefreshServiceImpl::CreateRegistrationRequest(
    BoundSessionRegistrationFetcherParam registration_params) {
  if (active_registration_request_) {
    // If there are multiple racing registration requests, only one will be
    // processed and it will contain the most up-to-date set of cookies.
    return;
  }

  active_registration_request_ =
      std::make_unique<BoundSessionRegistrationFetcherImpl>(
          std::move(registration_params),
          storage_partition_->GetURLLoaderFactoryForBrowserProcess(),
          &key_service_.get());
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

void BoundSessionCookieRefreshServiceImpl::OnRegistrationRequestComplete(
    absl::optional<bound_session_credentials::RegistrationParams>
        registration_params) {
  if (registration_params.has_value()) {
    RegisterNewBoundSession(*registration_params);
  }

  active_registration_request_.reset();
}

void BoundSessionCookieRefreshServiceImpl::
    OnBoundSessionThrottlerParamsChanged() {
  UpdateAllRenderers();
}

void BoundSessionCookieRefreshServiceImpl::TerminateSession() {
  cookie_controller_.reset();
  session_params_storage_->ClearParams();
  UpdateAllRenderers();
}

std::unique_ptr<BoundSessionCookieController>
BoundSessionCookieRefreshServiceImpl::CreateBoundSessionCookieController(
    const bound_session_credentials::RegistrationParams& registration_params,
    const base::flat_set<std::string>& cookie_names) {
  return controller_factory_for_testing_.is_null()
             ? std::make_unique<BoundSessionCookieControllerImpl>(
                   key_service_.get(), storage_partition_,
                   network_connection_tracker_, registration_params,
                   cookie_names, this)
             : controller_factory_for_testing_.Run(registration_params,
                                                   cookie_names, this);
}

void BoundSessionCookieRefreshServiceImpl::InitializeBoundSession(
    const bound_session_credentials::RegistrationParams& registration_params) {
  CHECK(!cookie_controller_);
  constexpr char k1PSIDTSCookieName[] = "__Secure-1PSIDTS";
  constexpr char k3PSIDTSCookieName[] = "__Secure-3PSIDTS";

  cookie_controller_ = CreateBoundSessionCookieController(
      registration_params, {k1PSIDTSCookieName, k3PSIDTSCookieName});
  cookie_controller_->Initialize();
  UpdateAllRenderers();
}

void BoundSessionCookieRefreshServiceImpl::UpdateAllRenderers() {
  if (renderer_updater_) {
    renderer_updater_.Run();
  }
}
