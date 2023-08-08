// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_refresh_service_impl.h"

#include <memory>

#include "base/base64.h"
#include "base/check.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_controller.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_controller_impl.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_registration_fetcher_impl.h"
#include "chrome/common/renderer_configuration.mojom.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_client.h"
#include "google_apis/gaia/gaia_urls.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace {
const char kRegistrationParamsPref[] =
    "bound_session_credentials_registration_params";
const char kGoogleSessionTerminationHeader[] = "Sec-Session-Google-Termination";
}

BoundSessionCookieRefreshServiceImpl::BoundSessionCookieRefreshServiceImpl(
    unexportable_keys::UnexportableKeyService& key_service,
    PrefService* pref_service,
    SigninClient* client)
    : key_service_(key_service), pref_service_(pref_service), client_(client) {}

BoundSessionCookieRefreshServiceImpl::~BoundSessionCookieRefreshServiceImpl() =
    default;

// static
void BoundSessionCookieRefreshServiceImpl::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterStringPref(kRegistrationParamsPref, std::string());
}

void BoundSessionCookieRefreshServiceImpl::Initialize() {
  OnBoundSessionUpdated();
}

void BoundSessionCookieRefreshServiceImpl::RegisterNewBoundSession(
    const bound_session_credentials::RegistrationParams& params) {
  if (!IsValidRegistrationParams(params) ||
      !PersistRegistrationParams(params)) {
    DVLOG(1) << "Invalid session params or failed to serialize bound session "
                "registration params.";
    return;
  }
  // New session should override an existing one.
  ResetBoundSession();

  OnBoundSessionUpdated();
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

bool BoundSessionCookieRefreshServiceImpl::IsBoundSession() const {
  return pref_service_->HasPrefPath(kRegistrationParamsPref);
}

chrome::mojom::BoundSessionParamsPtr
BoundSessionCookieRefreshServiceImpl::GetBoundSessionParams() const {
  if (!cookie_controller_) {
    return chrome::mojom::BoundSessionParamsPtr();
  }

  return cookie_controller_->bound_session_params();
}

void BoundSessionCookieRefreshServiceImpl::
    SetRendererBoundSessionParamsUpdaterDelegate(
        RendererBoundSessionParamsUpdaterDelegate renderer_updater) {
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
  if (!IsBoundSession()) {
    // Session has been terminated.
    std::move(resume_blocked_request).Run();
    return;
  }
  CHECK(cookie_controller_);
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
          std::move(registration_params), client_->GetURLLoaderFactory(),
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

bool BoundSessionCookieRefreshServiceImpl::IsValidRegistrationParams(
    const bound_session_credentials::RegistrationParams& registration_params) {
  // TODO(crbug.com/1441168): Check for validity of other fields once they are
  // available.
  return registration_params.has_session_id() &&
         registration_params.has_wrapped_key();
}

bool BoundSessionCookieRefreshServiceImpl::PersistRegistrationParams(
    const bound_session_credentials::RegistrationParams& registration_params) {
  std::string serialized_params = registration_params.SerializeAsString();
  if (serialized_params.empty()) {
    return false;
  }

  std::string encoded_serialized_params;
  base::Base64Encode(serialized_params, &encoded_serialized_params);
  pref_service_->SetString(kRegistrationParamsPref, encoded_serialized_params);
  return true;
}

absl::optional<bound_session_credentials::RegistrationParams>
BoundSessionCookieRefreshServiceImpl::GetRegistrationParams() {
  std::string encoded_params_str =
      pref_service_->GetString(kRegistrationParamsPref);
  if (encoded_params_str.empty()) {
    return absl::nullopt;
  }

  std::string params_str;
  if (!base::Base64Decode(encoded_params_str, &params_str)) {
    return absl::nullopt;
  }

  bound_session_credentials::RegistrationParams params;
  if (params.ParseFromString(params_str) && IsValidRegistrationParams(params)) {
    return params;
  }
  return absl::nullopt;
}

void BoundSessionCookieRefreshServiceImpl::OnBoundSessionParamsChanged() {
  UpdateAllRenderers();
}

void BoundSessionCookieRefreshServiceImpl::TerminateSession() {
  pref_service_->ClearPref(kRegistrationParamsPref);
  OnBoundSessionUpdated();
}

std::unique_ptr<BoundSessionCookieController>
BoundSessionCookieRefreshServiceImpl::CreateBoundSessionCookieController(
    bound_session_credentials::RegistrationParams registration_params,
    const base::flat_set<std::string>& cookie_names) {
  return controller_factory_for_testing_.is_null()
             ? std::make_unique<BoundSessionCookieControllerImpl>(
                   key_service_.get(), client_, registration_params,
                   cookie_names, this)
             : controller_factory_for_testing_.Run(registration_params,
                                                   cookie_names, this);
}

void BoundSessionCookieRefreshServiceImpl::InitializeBoundSession() {
  CHECK(!cookie_controller_);
  constexpr char k1PSIDTSCookieName[] = "__Secure-1PSIDTS";
  constexpr char k3PSIDTSCookieName[] = "__Secure-3PSIDTS";

  absl::optional<bound_session_credentials::RegistrationParams>
      registration_params = GetRegistrationParams();
  if (!registration_params) {
    TerminateSession();
    return;
  }

  cookie_controller_ = CreateBoundSessionCookieController(
      registration_params.value(), {k1PSIDTSCookieName, k3PSIDTSCookieName});
  cookie_controller_->Initialize();
}

void BoundSessionCookieRefreshServiceImpl::ResetBoundSession() {
  cookie_controller_.reset();
}

void BoundSessionCookieRefreshServiceImpl::OnBoundSessionUpdated() {
  if (!IsBoundSession()) {
    ResetBoundSession();
  } else {
    InitializeBoundSession();
  }
  UpdateAllRenderers();
}

void BoundSessionCookieRefreshServiceImpl::UpdateAllRenderers() {
  if (renderer_updater_) {
    renderer_updater_.Run();
  }
}
