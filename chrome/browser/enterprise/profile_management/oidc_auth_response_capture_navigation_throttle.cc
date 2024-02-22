// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/profile_management/oidc_auth_response_capture_navigation_throttle.h"

#include <optional>
#include <string>

#include "base/base64url.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_split.h"
#include "chrome/browser/enterprise/profile_management/profile_management_features.h"
#include "chrome/browser/enterprise/signin/oidc_authentication_signin_interceptor.h"
#include "chrome/browser/enterprise/signin/oidc_authentication_signin_interceptor_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace {

constexpr char kQuerySeparator[] = "&";
constexpr char kOidcEnrollmentHost[] = "chromeprofiletoken";
constexpr char kAuthTokenHeader[] = "access_token=";
constexpr char kIdTokenHeader[] = "id_token=";

std::string ExtractFragmentValueWithKey(const std::string& fragment,
                                        const std::string& key) {
  size_t start = fragment.find(key);
  if (start == std::string::npos) {
    return std::string();
  }
  start += key.length();

  size_t end = fragment.find(kQuerySeparator, start);
  return (end == std::string::npos) ? fragment.substr(start)
                                    : fragment.substr(start, end - start);
}

}  // namespace

namespace profile_management {

// static
std::unique_ptr<OidcAuthResponseCaptureNavigationThrottle>
OidcAuthResponseCaptureNavigationThrottle::MaybeCreateThrottleFor(
    content::NavigationHandle* navigation_handle) {
  return (base::FeatureList::IsEnabled(
              profile_management::features::kOidcAuthProfileManagement) &&
          navigation_handle->GetURL().host() == kOidcEnrollmentHost)
             ? std::make_unique<OidcAuthResponseCaptureNavigationThrottle>(
                   navigation_handle)
             : nullptr;
}

OidcAuthResponseCaptureNavigationThrottle::
    OidcAuthResponseCaptureNavigationThrottle(
        content::NavigationHandle* navigation_handle)
    : content::NavigationThrottle(navigation_handle) {}

OidcAuthResponseCaptureNavigationThrottle::
    ~OidcAuthResponseCaptureNavigationThrottle() = default;

content::NavigationThrottle::ThrottleCheckResult
OidcAuthResponseCaptureNavigationThrottle::WillStartRequest() {
  return WillRedirectRequest();
}

content::NavigationThrottle::ThrottleCheckResult
OidcAuthResponseCaptureNavigationThrottle::WillRedirectRequest() {
  // Extract parameters from the fragment part (#) of the URL. The auth token
  // from OIDC authentication will be decoded and parsed by data_decoder for
  // security reasons. Example URL:
  // https://chromeprofiletoken/#access_token=<oauth_token>&token_type=Bearer&expires_in=4887&scope=email+openid+profile&id_token=<id_token>&session_state=<session
  // state>
  std::string url_ref = navigation_handle()->GetURL().ref();

  std::string auth_token =
      ExtractFragmentValueWithKey(url_ref, kAuthTokenHeader);
  std::string id_token = ExtractFragmentValueWithKey(url_ref, kIdTokenHeader);

  if (auth_token.empty() || id_token.empty()) {
    LOG(ERROR) << "Missing token from OIDC response.";
    return CANCEL_AND_IGNORE;
  }

  std::string json_payload;
  std::vector<std::string_view> jwt_sections = base::SplitStringPiece(
      auth_token, ".", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  if (jwt_sections.size() != 3) {
    LOG(ERROR) << "Oauth token from OIDC response has Invalid JWT format.";
    return CANCEL_AND_IGNORE;
  }

  if (!base::Base64UrlDecode(jwt_sections[1],
                             base::Base64UrlDecodePolicy::IGNORE_PADDING,
                             &json_payload)) {
    LOG(ERROR) << "Oauth token payload from OIDC response can't be decoded.";
    return CANCEL_AND_IGNORE;
  }

  data_decoder::DataDecoder::ParseJsonIsolated(
      json_payload,
      base::BindOnce(
          &OidcAuthResponseCaptureNavigationThrottle::RegisterWithOidcTokens,
          weak_ptr_factory_.GetWeakPtr(),
          ProfileManagementOicdTokens{.auth_token = std::move(auth_token),
                                      .id_token = std::move(id_token)}));
  return DEFER;
}

const char* OidcAuthResponseCaptureNavigationThrottle::GetNameForLogging() {
  return "OidcAuthResponseCaptureNavigationThrottle";
}

void OidcAuthResponseCaptureNavigationThrottle::RegisterWithOidcTokens(
    ProfileManagementOicdTokens tokens,
    data_decoder::DataDecoder::ValueOrError result) {
  if (!result.has_value()) {
    LOG(ERROR) << "Failed to parse decoded Oauth token payload.";
    return;
  }
  const base::Value::Dict* parsed_json = result->GetIfDict();

  if (!parsed_json) {
    LOG(ERROR) << "Decoded Oauth token payload is empty.";
    return;
  }

  const std::string* user_email = parsed_json->FindString("upn");
  if (!user_email || (*user_email).empty()) {
    LOG(ERROR) << "User email not found in token payload.";
    return;
  }

  auto* interceptor = OidcAuthenticationSigninInterceptorFactory::GetForProfile(
      Profile::FromBrowserContext(
          navigation_handle()->GetWebContents()->GetBrowserContext()));

  interceptor->MaybeInterceptOidcAuthentication(
      navigation_handle()->GetWebContents(), tokens, *user_email);
}

}  // namespace profile_management
