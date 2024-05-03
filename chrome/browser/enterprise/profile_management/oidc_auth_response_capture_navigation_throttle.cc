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
#include "components/policy/core/common/policy_logger.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace {

constexpr char kEnrollmentFallbackHost[] = "chromeenterprise.google";
constexpr char kEnrollmentFallbackPath[] = "/enroll/";

// Msft Entra will first navigate to a reprocess URL and redirect to our
// enrolllment URL, we need to capture this to correctly create the navigation
// throttle.
constexpr char kOidcEntraLoginHost[] = "login.microsoftonline.com";
constexpr char kOidcEntraReprocessPath[] = "/common/reprocess";
constexpr char kOidcEntraLoginPath[] = "/common/login";
// For new identities, the redirection starts from the "Keep me signed in" page.
constexpr char kOidcEntraKmsiPath[] = "/kmsi";

constexpr char kQuerySeparator[] = "&";
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

bool IsEnrollmentUrl(GURL& url) {
  return url.DomainIs(kEnrollmentFallbackHost) &&
         url.path() == kEnrollmentFallbackPath;
}

}  // namespace

namespace profile_management {

// static
std::unique_ptr<OidcAuthResponseCaptureNavigationThrottle>
OidcAuthResponseCaptureNavigationThrottle::MaybeCreateThrottleFor(
    content::NavigationHandle* navigation_handle) {
  if (!base::FeatureList::IsEnabled(
          profile_management::features::kOidcAuthProfileManagement)) {
    return nullptr;
  }

  auto url = navigation_handle->GetURL();
  if (!(url.DomainIs(kOidcEntraLoginHost) &&
        (url.path() == kOidcEntraReprocessPath ||
         url.path() == kOidcEntraKmsiPath ||
         url.path() == kOidcEntraLoginPath))) {
    return nullptr;
  }

  VLOG_POLICY(2, OIDC_ENROLLMENT)
      << "Valid enrollment URL found, processing URL: " << url;
  return std::make_unique<OidcAuthResponseCaptureNavigationThrottle>(
      navigation_handle);
}

OidcAuthResponseCaptureNavigationThrottle::
    OidcAuthResponseCaptureNavigationThrottle(
        content::NavigationHandle* navigation_handle)
    : content::NavigationThrottle(navigation_handle) {}

OidcAuthResponseCaptureNavigationThrottle::
    ~OidcAuthResponseCaptureNavigationThrottle() = default;

content::NavigationThrottle::ThrottleCheckResult
OidcAuthResponseCaptureNavigationThrottle::WillRedirectRequest() {
  return AttemptToTriggerInterception();
}

content::NavigationThrottle::ThrottleCheckResult
OidcAuthResponseCaptureNavigationThrottle::WillProcessResponse() {
  return AttemptToTriggerInterception();
}

content::NavigationThrottle::ThrottleCheckResult
OidcAuthResponseCaptureNavigationThrottle::AttemptToTriggerInterception() {
  if (interception_triggered_) {
    return PROCEED;
  }
  auto url = navigation_handle()->GetURL();

  // This maybe some other redirect from MSFT Entra that isn't an OIDC profile
  // registration attempt.
  if (!IsEnrollmentUrl(url)) {
    VLOG_POLICY(1, OIDC_ENROLLMENT)
        << "Enrollment URL from OIDC redircetion is invalid: " << url;
    return PROCEED;
  }

  // Extract parameters from the fragment part (#) of the URL. The auth token
  // from OIDC authentication will be decoded and parsed by data_decoder for
  // security reasons. Example URL:
  // https://chromeprofiletoken/#access_token=<oauth_token>&token_type=Bearer&expires_in=4887&scope=email+openid+profile&id_token=<id_token>&session_state=<session
  // state>
  std::string url_ref = url.ref();

  std::string auth_token =
      ExtractFragmentValueWithKey(url_ref, kAuthTokenHeader);
  std::string id_token = ExtractFragmentValueWithKey(url_ref, kIdTokenHeader);

  if (auth_token.empty() || id_token.empty()) {
    LOG_POLICY(ERROR, OIDC_ENROLLMENT)
        << "Tokens missing from OIDC Redirection URL";
    return PROCEED;
  }

  std::string json_payload;
  std::vector<std::string_view> jwt_sections = base::SplitStringPiece(
      id_token, ".", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  if (jwt_sections.size() != 3) {
    LOG_POLICY(ERROR, OIDC_ENROLLMENT)
        << "Oauth token from OIDC response has Invalid JWT format.";
    return CANCEL_AND_IGNORE;
  }

  if (!base::Base64UrlDecode(jwt_sections[1],
                             base::Base64UrlDecodePolicy::IGNORE_PADDING,
                             &json_payload)) {
    LOG_POLICY(ERROR, OIDC_ENROLLMENT)
        << "Oauth token payload from OIDC response can't be decoded.";
    return CANCEL_AND_IGNORE;
  }

  interception_triggered_ = true;
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
    LOG_POLICY(ERROR, OIDC_ENROLLMENT)
        << "Failed to parse decoded Oauth token payload.";
    return Resume();
  }
  const base::Value::Dict* parsed_json = result->GetIfDict();

  if (!parsed_json) {
    LOG_POLICY(ERROR, OIDC_ENROLLMENT)
        << "Decoded Oauth token payload is empty.";
    return Resume();
  }

  const std::string* subject_id = parsed_json->FindString("sub");
  if (!subject_id || (*subject_id).empty()) {
    LOG_POLICY(ERROR, OIDC_ENROLLMENT)
        << "Subject ID is missing in token payload.";
    return Resume();
  }

  auto* interceptor = OidcAuthenticationSigninInterceptorFactory::GetForProfile(
      Profile::FromBrowserContext(
          navigation_handle()->GetWebContents()->GetBrowserContext()));

  VLOG_POLICY(2, OIDC_ENROLLMENT)
      << "OIDC redirection meets all requirements, starting enrollment "
         "process.";
  interceptor->MaybeInterceptOidcAuthentication(
      navigation_handle()->GetWebContents(), tokens, *subject_id,
      base::BindOnce(&OidcAuthResponseCaptureNavigationThrottle::Resume,
                     weak_ptr_factory_.GetWeakPtr()));
}

}  // namespace profile_management
