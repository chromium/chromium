// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/bound_session_oauth_multilogin_delegate_impl.h"

#include "base/check_deref.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_refresh_service.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_params.pb.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_params_util.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/oauth_multilogin_result.h"

namespace {

GURL ConvertDeviceBoundSessionDomainToUrl(
    OAuthMultiloginResult::DeviceBoundSession::Domain domain) {
  using enum OAuthMultiloginResult::DeviceBoundSession::Domain;
  switch (domain) {
    case kGoogle:
      return GURL("https://google.com");
    case kUnknown:
      // This shouldn't happen as unknown domains should be filtered out before
      // (at server response parsing).
      NOTREACHED();
  }
}

}  // namespace

BoundSessionOAuthMultiLoginDelegateImpl::
    BoundSessionOAuthMultiLoginDelegateImpl(
        base::WeakPtr<BoundSessionCookieRefreshService>
            bound_session_cookie_refresh_service,
        const signin::IdentityManager* identity_manager)
    : bound_session_cookie_refresh_service_(
          bound_session_cookie_refresh_service),
      identity_manager_(CHECK_DEREF(identity_manager)) {}

BoundSessionOAuthMultiLoginDelegateImpl::
    ~BoundSessionOAuthMultiLoginDelegateImpl() = default;

void BoundSessionOAuthMultiLoginDelegateImpl::BeforeSetCookies(
    const OAuthMultiloginResult& result) {
  CHECK_EQ(result.status(), OAuthMultiloginResponseStatus::kOk);
  if (!bound_session_cookie_refresh_service_) {
    bound_sessions_params_.emplace();
    return;
  }
  std::vector<bound_session_credentials::BoundSessionParams>
      bound_sessions_params = CreateBoundSessionsParams(result);
  for (const auto& params : bound_sessions_params) {
    bound_session_cookie_refresh_service_->StopCookieRotation(
        bound_session_credentials::GetBoundSessionKey(params));
  }
  bound_sessions_params_ = std::move(bound_sessions_params);
}

void BoundSessionOAuthMultiLoginDelegateImpl::OnCookiesSet() {
  // `BeforeSetCookies` must have been called.
  CHECK(bound_sessions_params_.has_value());
  if (!bound_session_cookie_refresh_service_) {
    bound_sessions_params_.reset();
    return;
  }
  for (const auto& params : *bound_sessions_params_) {
    bound_session_cookie_refresh_service_->RegisterNewBoundSession(params);
  }
  base::UmaHistogramCounts100(
      "Signin.BoundSessionCredentials.OAuthMultilogin.RegisteredSessions",
      bound_sessions_params_->size());
  bound_sessions_params_.reset();
}

std::vector<bound_session_credentials::BoundSessionParams>
BoundSessionOAuthMultiLoginDelegateImpl::CreateBoundSessionsParams(
    const OAuthMultiloginResult& result) {
  std::vector<const OAuthMultiloginResult::DeviceBoundSession*>
      sessions_to_register = result.GetDeviceBoundSessionsToRegister();
  if (sessions_to_register.empty()) {
    return {};
  }
  const std::vector<uint8_t> wrapped_binding_key =
      identity_manager_->GetWrappedBindingKey();
  if (wrapped_binding_key.empty()) {
    // This should not happen as OAuthMultilogin should return bound cookies
    // only if the client exchanged bound LST.
    base::UmaHistogramBoolean(
        "Signin.BoundSessionCredentials.OAuthMultilogin.BindingKeyMissing",
        true);
    return {};
  }
  const std::string wrapped_binding_key_str(wrapped_binding_key.begin(),
                                            wrapped_binding_key.end());
  int invalid_params_count = 0;
  std::vector<bound_session_credentials::BoundSessionParams>
      bound_sessions_params;
  for (const auto* device_bound_session : sessions_to_register) {
    bound_session_credentials::BoundSessionParams params =
        bound_session_credentials::
            CreateBoundSessionsParamsFromRegistrationPayload(
                *device_bound_session->register_session_payload,
                GaiaUrls::GetInstance()->oauth_multilogin_url(),
                ConvertDeviceBoundSessionDomainToUrl(
                    device_bound_session->domain),
                wrapped_binding_key_str,
                bound_session_credentials::SessionOrigin::SESSION_ORIGIN_OAML);
    if (!bound_session_credentials::AreParamsValid(params)) {
      ++invalid_params_count;
      continue;
    }
    bound_sessions_params.push_back(std::move(params));
  }
  base::UmaHistogramCounts100(
      "Signin.BoundSessionCredentials.OAuthMultilogin.InvalidParams",
      invalid_params_count);
  return bound_sessions_params;
}
