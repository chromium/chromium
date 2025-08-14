// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/bound_session_oauth_multilogin_delegate_impl.h"

#include "base/check_deref.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_refresh_service.h"
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
    case kYoutube:
      return GURL("https://youtube.com");
    case kUnknown:
      return GURL();
  }
}

// Returns the list of bound sessions that need to be registered.
//
// Returning a vector of pointers to avoid copying the registration payload. The
// output vector is guaranteed to contain pointers to the input vector elements.
std::vector<const OAuthMultiloginResult::DeviceBoundSession*>
GetBoundSessionsToRegister(
    const std::vector<OAuthMultiloginResult::DeviceBoundSession>&
        bound_sessions) {
  std::vector<const OAuthMultiloginResult::DeviceBoundSession*>
      bound_sessions_to_register;
  for (const OAuthMultiloginResult::DeviceBoundSession& device_bound_session :
       bound_sessions) {
    if (device_bound_session.is_device_bound &&
        device_bound_session.register_session_payload.has_value()) {
      bound_sessions_to_register.push_back(&device_bound_session);
    }
  }
  return bound_sessions_to_register;
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
  if (result.status() != OAuthMultiloginResponseStatus::kOk) {
    return;
  }
  std::vector<bound_session_credentials::BoundSessionParams>
      bound_sessions_params = CreateBoundSessionsParams(result);
  // TODO(https://crbug.com/312719798): Pause the cookies rotations until the
  // cookies are set.
}

void BoundSessionOAuthMultiLoginDelegateImpl::OnCookiesSet() {
  // TODO(msalama): Start/Override Google DBSC session if needed.
}

std::vector<bound_session_credentials::BoundSessionParams>
BoundSessionOAuthMultiLoginDelegateImpl::CreateBoundSessionsParams(
    const OAuthMultiloginResult& result) {
  std::vector<const OAuthMultiloginResult::DeviceBoundSession*>
      sessions_to_register =
          GetBoundSessionsToRegister(result.device_bound_sessions());
  if (sessions_to_register.empty()) {
    return {};
  }
  const std::vector<uint8_t> wrapped_binding_key =
      identity_manager_->GetWrappedBindingKey();
  if (wrapped_binding_key.empty()) {
    // This should not happen as OAuthMultilogin should return bound cookies
    // only if the client exchanged bound LST.
    // TODO(crbug.com/312719798): Add a histogram to track this.
    return {};
  }
  const std::string wrapped_binding_key_str(wrapped_binding_key.begin(),
                                            wrapped_binding_key.end());
  std::vector<bound_session_credentials::BoundSessionParams>
      bound_sessions_params;
  for (const auto* device_bound_session : sessions_to_register) {
    const GURL site =
        ConvertDeviceBoundSessionDomainToUrl(device_bound_session->domain);
    if (!site.is_valid()) {
      // This can happen if the client is not aware of the new domain (e.g.
      // outdated version).
      //
      // TODO(crbug.com/312719798): Add a histogram to track this.
      continue;
    }
    bound_session_credentials::BoundSessionParams params =
        bound_session_credentials::
            CreateBoundSessionsParamsFromRegistrationPayload(
                *device_bound_session->register_session_payload,
                GaiaUrls::GetInstance()->oauth_multilogin_url(), site,
                wrapped_binding_key_str);
    if (!bound_session_credentials::AreParamsValid(params)) {
      // TODO(crbug.com/312719798): Add a histogram to track this.
      continue;
    }
    bound_sessions_params.push_back(std::move(params));
  }
  return bound_sessions_params;
}
