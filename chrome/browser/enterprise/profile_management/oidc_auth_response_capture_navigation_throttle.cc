// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/profile_management/oidc_auth_response_capture_navigation_throttle.h"

#include <optional>
#include <string>
#include <vector>

#include "base/base64url.h"
#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_split.h"
#include "chrome/browser/enterprise/profile_management/profile_management_features.h"
#include "chrome/browser/enterprise/signin/oidc_authentication_signin_interceptor.h"
#include "chrome/browser/enterprise/signin/oidc_authentication_signin_interceptor_factory.h"
#include "chrome/browser/enterprise/signin/oidc_metrics_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "components/policy/core/common/policy_logger.h"
#include "components/url_matcher/url_util.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "url/gurl.h"

using url_matcher::URLMatcher;

namespace {

constexpr char kEnrollmentFallbackUrl[] =
    "https://chromeenterprise.google/enroll";

// We consider this common host for Microsoft authentication to be valid
// redirection source.
constexpr char kEntraLoginHost[] = "https://login.microsoftonline.com";
// Valid redirection from MSFT Cloud App Security portal.
constexpr char kEntraMcasHost[] = "https://mcas.ms";

constexpr char kQuerySeparator[] = "&";
constexpr char kKeyValueSeparator[] = "=";
constexpr char kAuthTokenHeader[] = "access_token";
constexpr char kIdTokenHeader[] = "id_token";
constexpr char kOidcStateHeader[] = "state";

base::flat_map<std::string, std::string> SplitUrl(const std::string& url) {
  std::vector<std::string> fragments = base::SplitString(
      url, kQuerySeparator, base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  base::flat_map<std::string, std::string> url_map;
  for (auto& fragment : fragments) {
    size_t start = fragment.find(kKeyValueSeparator);
    if (start == std::string::npos) {
      continue;
    }
    std::string key = fragment.substr(0, start);
    std::string val = fragment.substr(start + 1, fragment.size());
    url_map.emplace(key, val);
  }

  return url_map;
}

std::unique_ptr<URLMatcher> CreateEnrollmentRedirectUrlMatcher() {
  auto matcher = std::make_unique<URLMatcher>();
  url_matcher::util::AddAllowFilters(
      matcher.get(), std::vector<std::string>({kEnrollmentFallbackUrl}));
  return matcher;
}

const url_matcher::URLMatcher* GetEnrollmentRedirectUrlMatcher() {
  static base::NoDestructor<std::unique_ptr<URLMatcher>> matcher(
      CreateEnrollmentRedirectUrlMatcher());
  return matcher->get();
}

bool IsEnrollmentUrl(GURL& url) {
  return !GetEnrollmentRedirectUrlMatcher()->MatchURL(url).empty();
}

std::unique_ptr<URLMatcher> CreateOidcEnrollmentUrlMatcher() {
  auto matcher = std::make_unique<URLMatcher>();

  std::vector<std::string> allowed_hosts({kEntraLoginHost, kEntraMcasHost});
  if (base::FeatureList::IsEnabled(
          profile_management::features::kOidcEnrollmentAuthSource)) {
    const std::vector<std::string>& hosts = base::SplitString(
        profile_management::features::kOidcAuthAdditionalHosts.Get(), ",",
        base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

    for (std::string host : hosts) {
      allowed_hosts.push_back(host);
    }
  }

  url_matcher::util::AddAllowFilters(matcher.get(), allowed_hosts);
  return matcher;
}

const url_matcher::URLMatcher* GetOidcEnrollmentUrlMatcher() {
  static base::NoDestructor<std::unique_ptr<URLMatcher>> matcher(
      CreateOidcEnrollmentUrlMatcher());
  return matcher->get();
}

void RecordUntrustedRedirectChain(
    content::NavigationHandle& navigation_handle) {
  ukm::SourceId source_id = ukm::ConvertToSourceId(
      navigation_handle.GetNavigationId(), ukm::SourceIdType::NAVIGATION_ID);
  ukm::builders::Enterprise_Profile_Enrollment(source_id)
      .SetIsUntrustedOidcRedirect(true)
      .Record(ukm::UkmRecorder::Get());
}

}  // namespace

namespace profile_management {

// static
std::unique_ptr<OidcAuthResponseCaptureNavigationThrottle>
OidcAuthResponseCaptureNavigationThrottle::MaybeCreateThrottleFor(
    content::NavigationHandle* navigation_handle) {
  if (base::FeatureList::IsEnabled(
          profile_management::features::kOidcAuthProfileManagement) &&
      navigation_handle->IsInMainFrame()) {
    return std::make_unique<OidcAuthResponseCaptureNavigationThrottle>(
        navigation_handle);
  }

  return nullptr;
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
  return (base::FeatureList::IsEnabled(
             profile_management::features::kOidcAuthResponseInterception))
             ? AttemptToTriggerInterception()
             : PROCEED;
}

const char* OidcAuthResponseCaptureNavigationThrottle::GetNameForLogging() {
  return "OidcAuthResponseCaptureNavigationThrottle";
}

// static
std::unique_ptr<URLMatcher> OidcAuthResponseCaptureNavigationThrottle::
    GetOidcEnrollmentUrlMatcherForTesting() {
  return CreateOidcEnrollmentUrlMatcher();
}

content::NavigationThrottle::ThrottleCheckResult
OidcAuthResponseCaptureNavigationThrottle::AttemptToTriggerInterception() {
  if (interception_triggered_) {
    return PROCEED;
  }

  if (navigation_handle()->GetRedirectChain().empty()) {
    return PROCEED;
  }

  auto url = navigation_handle()->GetURL();
  // Only try kicking off OIDC enrollment process if a valid enroll URL is seen.
  if (!IsEnrollmentUrl(url)) {
    return PROCEED;
  }

  VLOG_POLICY(1, OIDC_ENROLLMENT)
      << "Valid enrollment URL from OIDC redirection is found: " << url;

  if (!base::FeatureList::IsEnabled(
          profile_management::features::
              kEnableGenericOidcAuthProfileManagement)) {
    bool accept_redirect = false;

    for (const auto& chain_url : navigation_handle()->GetRedirectChain()) {
      if (!GetOidcEnrollmentUrlMatcher()->MatchURL(chain_url).empty()) {
        accept_redirect = true;
        break;
      }
    }

    if (!accept_redirect) {
      RecordUntrustedRedirectChain(*navigation_handle());
      VLOG_POLICY(1, OIDC_ENROLLMENT)
          << "Enrollment flow cannot be initiated due to an untrusted chain of "
             "redirects.";
      return PROCEED;
    }
  }

  RecordOidcInterceptionFunnelStep(
      OidcInterceptionFunnelStep::kValidRedirectionCaptured);

  auto* profile = Profile::FromBrowserContext(
      navigation_handle()->GetWebContents()->GetBrowserContext());
  // OIDC enrollment cannot be initiated from an incognito or guest profile.
  if (!profile || profile->IsOffTheRecord() || profile->IsGuestSession()) {
    RecordOidcInterceptionResult(OidcInterceptionResult::kInvalidProfile);
    VLOG_POLICY(1, OIDC_ENROLLMENT)
        << "Enrollment flow cannot be initiated from OTR profile.";
    return PROCEED;
  }

  // Extract parameters from the fragment part (#) of the URL. The auth token
  // from OIDC authentication will be decoded and parsed by data_decoder for
  // security reasons. Example URL:
  // https://chromeenterprise.google/enroll/#access_token=<oauth_token>&token_type=Bearer&expires_in=4887&scope=email+openid+profile&id_token=<id_token>&session_state=<session_state>
  std::string url_ref = url.ref();
  base::flat_map<std::string, std::string> url_map = SplitUrl(url_ref);
  if (url_map.size() == 0) {
    VLOG_POLICY(1, OIDC_ENROLLMENT)
        << "Failed to extract details from the enrollment URL: " << url;
    return PROCEED;
  }

  // In the case that we are preforming a generic OIDC profile enrollment, an
  // additional OIDC state field is present in the URL.
  std::string state = "";
  if (base::FeatureList::IsEnabled(
          profile_management::features::
              kEnableGenericOidcAuthProfileManagement)) {
    if (url_map.contains(kOidcStateHeader)) {
      state = url_map[kOidcStateHeader];
    } else {
      LOG_POLICY(WARNING, OIDC_ENROLLMENT)
          << "OIDC state is missing from the OIDC enrollment URL.";
    }
  }

  std::string auth_token =
      url_map.contains(kAuthTokenHeader) ? url_map[kAuthTokenHeader] : "";
  std::string id_token =
      url_map.contains(kIdTokenHeader) ? url_map[kIdTokenHeader] : "";

  if (auth_token.empty() || id_token.empty()) {
    LOG_POLICY(ERROR, OIDC_ENROLLMENT)
        << "Tokens missing from OIDC Redirection URL";
    RecordOidcInterceptionResult(OidcInterceptionResult::kInvalidUrlOrTokens);
    return PROCEED;
  }

  std::string json_payload;
  std::vector<std::string_view> jwt_sections = base::SplitStringPiece(
      id_token, ".", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  if (jwt_sections.size() != 3) {
    LOG_POLICY(ERROR, OIDC_ENROLLMENT)
        << "Oauth token from OIDC response has Invalid JWT format.";
    RecordOidcInterceptionResult(OidcInterceptionResult::kInvalidUrlOrTokens);
    return CANCEL_AND_IGNORE;
  }

  if (!base::Base64UrlDecode(jwt_sections[1],
                             base::Base64UrlDecodePolicy::IGNORE_PADDING,
                             &json_payload)) {
    LOG_POLICY(ERROR, OIDC_ENROLLMENT)
        << "Oauth token payload from OIDC response can't be decoded.";
    RecordOidcInterceptionResult(OidcInterceptionResult::kInvalidUrlOrTokens);
    return CANCEL_AND_IGNORE;
  }

  interception_triggered_ = true;
  data_decoder::DataDecoder::ParseJsonIsolated(
      json_payload,
      base::BindOnce(
          &OidcAuthResponseCaptureNavigationThrottle::RegisterWithOidcTokens,
          weak_ptr_factory_.GetWeakPtr(),
          ProfileManagementOidcTokens(std::move(auth_token),
                                      std::move(id_token), std::move(state))));
  return DEFER;
}

void OidcAuthResponseCaptureNavigationThrottle::RegisterWithOidcTokens(
    ProfileManagementOidcTokens tokens,
    data_decoder::DataDecoder::ValueOrError result) {
  if (!result.has_value()) {
    LOG_POLICY(ERROR, OIDC_ENROLLMENT)
        << "Failed to parse decoded Oauth token payload.";
    RecordOidcInterceptionResult(OidcInterceptionResult::kInvalidUrlOrTokens);
    return Resume();
  }
  const base::Value::Dict* parsed_json = result->GetIfDict();

  if (!parsed_json) {
    LOG_POLICY(ERROR, OIDC_ENROLLMENT)
        << "Decoded Oauth token payload is empty.";
    RecordOidcInterceptionResult(OidcInterceptionResult::kInvalidUrlOrTokens);
    return Resume();
  }

  const std::string* subject_id = parsed_json->FindString("sub");
  const std::string* issuer_id = parsed_json->FindString("iss");
  if (!subject_id || (*subject_id).empty()) {
    LOG_POLICY(ERROR, OIDC_ENROLLMENT)
        << "Subject ID is missing in token payload.";
    RecordOidcInterceptionResult(OidcInterceptionResult::kInvalidUrlOrTokens);
    return Resume();
  }

  if (!issuer_id || (*issuer_id).empty()) {
    LOG_POLICY(ERROR, OIDC_ENROLLMENT)
        << "Issuer identifier is missing in token payload.";
    RecordOidcInterceptionResult(OidcInterceptionResult::kInvalidUrlOrTokens);
    return Resume();
  }

  auto* interceptor = OidcAuthenticationSigninInterceptorFactory::GetForProfile(
      Profile::FromBrowserContext(
          navigation_handle()->GetWebContents()->GetBrowserContext()));

  VLOG_POLICY(2, OIDC_ENROLLMENT)
      << "OIDC redirection meets all requirements, starting enrollment "
         "process.";
  RecordOidcInterceptionFunnelStep(
      OidcInterceptionFunnelStep::kSuccessfulInfoParsed);

  interceptor->MaybeInterceptOidcAuthentication(
      navigation_handle()->GetWebContents(), tokens, *issuer_id, *subject_id,
      base::BindOnce(&OidcAuthResponseCaptureNavigationThrottle::Resume,
                     weak_ptr_factory_.GetWeakPtr()));
}

}  // namespace profile_management
