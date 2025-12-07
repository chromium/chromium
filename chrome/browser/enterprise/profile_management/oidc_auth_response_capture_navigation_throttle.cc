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
#include "base/version_info/channel.h"
#include "chrome/browser/enterprise/profile_management/profile_management_features.h"
#include "chrome/browser/enterprise/signin/oidc_authentication_signin_interceptor.h"
#include "chrome/browser/enterprise/signin/oidc_authentication_signin_interceptor_factory.h"
#include "chrome/browser/enterprise/signin/oidc_metrics_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/channel_info.h"
#include "components/policy/core/common/policy_logger.h"
#include "components/url_matcher/url_util.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/web_contents.h"
#include "net/http/http_response_headers.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "url/gurl.h"

using ThrottleCheckResult = content::NavigationThrottle::ThrottleCheckResult;
using url_matcher::URLMatcher;

namespace {

constexpr char kEnrollmentFallbackUrl[] =
    "https://chromeenterprise.google/enroll";

// We consider this common host for Microsoft authentication to be valid
// redirection source.
constexpr char kEntraLoginHost[] = "https://login.microsoftonline.com";
// Valid redirection from MSFT Cloud App Security portal.
constexpr char kEntraMcasHost[] = "https://mcas.ms";

// Chrome Enterprise page that handles OIDC authentication redirection, this
// page should receive the proper payload in its auth header to start OIDC
// profile creation/registration.
constexpr char kEnterpriseOidcRegisterUrl[] =
    "https://chromeenterprise.google/profile-enrollment/register-handler";

constexpr char kRegistrationHeaderField[] = "X-Profile-Registration-Payload";

constexpr char kQuerySeparator[] = "&";
constexpr char kKeyValueSeparator[] = "=";
constexpr char kAuthTokenHeader[] = "access_token";
constexpr char kIdTokenHeader[] = "id_token";
constexpr char kOidcStateHeader[] = "state";

constexpr char kPayloadIssuerFieldName[] = "issuer";
constexpr char kPayloadSubjectFieldName[] = "subject";
constexpr char kPayloadCodeFieldName[] = "encrypted_user_information";

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
  url_matcher::util::AddAllowFiltersWithLimit(
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

std::unique_ptr<URLMatcher> CreateEnrollmentHeaderUrlMatcher() {
  auto matcher = std::make_unique<URLMatcher>();

  std::vector<std::string> allowed_urls({kEnterpriseOidcRegisterUrl});

  // Inserting more supported URLs should be only available on Canary and Dev
  // for security.
  auto channel = chrome::GetChannel();
  if (channel != version_info::Channel::STABLE &&
      channel != version_info::Channel::BETA) {
    const std::vector<std::string>& urls = base::SplitString(
        profile_management::features::kOidcAuthAdditionalUrls.Get(), ",",
        base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

    for (const std::string& url : urls) {
      allowed_urls.push_back(url);
    }
  }

  url_matcher::util::AddAllowFiltersWithLimit(matcher.get(), allowed_urls);
  return matcher;
}

const url_matcher::URLMatcher* GetEnrollmentHeaderUrlMatcher() {
  static base::NoDestructor<std::unique_ptr<URLMatcher>> matcher(
      CreateEnrollmentHeaderUrlMatcher());
  return matcher->get();
}

bool IsEnrollmentHeaderUrl(GURL& url) {
  if (!base::FeatureList::IsEnabled(
          profile_management::features::kOidcAuthHeaderInterception)) {
    return false;
  }

  return !GetEnrollmentHeaderUrlMatcher()->MatchURL(url).empty();
}

std::unique_ptr<URLMatcher> CreateOidcEnrollmentUrlMatcher() {
  auto matcher = std::make_unique<URLMatcher>();

  std::vector<std::string> allowed_hosts({kEntraLoginHost, kEntraMcasHost});
  if (base::FeatureList::IsEnabled(
          profile_management::features::kOidcEnrollmentAuthSource)) {
    const std::vector<std::string>& hosts = base::SplitString(
        profile_management::features::kOidcAuthAdditionalHosts.Get(), ",",
        base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

    for (const std::string& host : hosts) {
      allowed_hosts.push_back(host);
    }
  }

  url_matcher::util::AddAllowFiltersWithLimit(matcher.get(), allowed_hosts);
  return matcher;
}

const url_matcher::URLMatcher* GetOidcEnrollmentUrlMatcher() {
  static base::NoDestructor<std::unique_ptr<URLMatcher>> matcher(
      CreateOidcEnrollmentUrlMatcher());
  return matcher->get();
}

bool IsProfileValidForOidcEnrollment(Profile* profile) {
  // OIDC enrollment cannot be initiated from an incognito or guest profile.
  if (!profile || profile->IsOffTheRecord() || profile->IsGuestSession()) {
    RecordOidcInterceptionResult(OidcInterceptionResult::kInvalidProfile);
    VLOG_POLICY(1, OIDC_ENROLLMENT)
        << "Enrollment flow cannot be initiated from OTR profile.";
    return false;
  }

  return true;
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
void OidcAuthResponseCaptureNavigationThrottle::MaybeCreateAndAdd(
    content::NavigationThrottleRegistry& registry) {
  if (base::FeatureList::IsEnabled(
          profile_management::features::kOidcAuthProfileManagement) &&
      registry.GetNavigationHandle().IsInPrimaryMainFrame()) {
    registry.AddThrottle(
        std::make_unique<OidcAuthResponseCaptureNavigationThrottle>(registry));
  }
}

OidcAuthResponseCaptureNavigationThrottle::
    OidcAuthResponseCaptureNavigationThrottle(
        content::NavigationThrottleRegistry& registry)
    : content::NavigationThrottle(registry) {}

OidcAuthResponseCaptureNavigationThrottle::
    ~OidcAuthResponseCaptureNavigationThrottle() = default;

ThrottleCheckResult
OidcAuthResponseCaptureNavigationThrottle::WillRedirectRequest() {
  return AttemptToTriggerUrlInterception();
}

ThrottleCheckResult
OidcAuthResponseCaptureNavigationThrottle::WillProcessResponse() {
  ThrottleCheckResult header_enrollment_check_result = PROCEED;
  if (base::FeatureList::IsEnabled(
          profile_management::features::kOidcAuthHeaderInterception)) {
    header_enrollment_check_result = AttemptToTriggerHeaderInterception();
  }

  // Skip the URL interception attempt if a header interception was successful,
  // or if response capturing is not enabled.
  return (base::FeatureList::IsEnabled(
              profile_management::features::kOidcAuthResponseInterception) &&
          header_enrollment_check_result.action() == PROCEED)
             ? AttemptToTriggerUrlInterception()
             : header_enrollment_check_result;
}

const char* OidcAuthResponseCaptureNavigationThrottle::GetNameForLogging() {
  return "OidcAuthResponseCaptureNavigationThrottle";
}

// static
std::unique_ptr<URLMatcher> OidcAuthResponseCaptureNavigationThrottle::
    GetOidcEnrollmentUrlMatcherForTesting() {
  return CreateOidcEnrollmentUrlMatcher();
}

ThrottleCheckResult
OidcAuthResponseCaptureNavigationThrottle::AttemptToTriggerUrlInterception() {
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
  if (!IsProfileValidForOidcEnrollment(profile)) {
    return PROCEED;
  }

  // Extract parameters from the fragment part (#) of the URL. The auth token
  // from OIDC authentication will be decoded and parsed by data_decoder for
  // security reasons. Example URL:
  // https://chromeenterprise.google/enroll/#access_token=<oauth_token>&token_type=Bearer&expires_in=4887&scope=email+openid+profile&id_token=<id_token>&session_state=<session_state>
  std::string url_ref = url.GetRef();
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
      std::string(),
      base::BindOnce(&OidcAuthResponseCaptureNavigationThrottle::Resume,
                     weak_ptr_factory_.GetWeakPtr()));
}

ThrottleCheckResult OidcAuthResponseCaptureNavigationThrottle::
    AttemptToTriggerHeaderInterception() {
  if (interception_triggered_) {
    return PROCEED;
  }

  auto url = navigation_handle()->GetURL();
  if (!IsEnrollmentHeaderUrl(url)) {
    return PROCEED;
  }

  VLOG_POLICY(1, OIDC_ENROLLMENT)
      << "Valid header enrollment URL navigation is found: " << url;

  auto* profile = Profile::FromBrowserContext(
      navigation_handle()->GetWebContents()->GetBrowserContext());
  if (!IsProfileValidForOidcEnrollment(profile)) {
    return PROCEED;
  }

  RecordOidcInterceptionFunnelStep(
      OidcInterceptionFunnelStep::kValidRedirectionCaptured);

  // The navigation itself is valid, start trying to extract required
  // information from its headers.
  const net::HttpResponseHeaders* headers =
      navigation_handle()->GetResponseHeaders();

  if (!headers) {
    VLOG_POLICY(1, OIDC_ENROLLMENT)
        << "Cannot find response header from OIDC enrollment URL";
    RecordOidcInterceptionResult(OidcInterceptionResult::kInvalidUrlOrTokens);
    return PROCEED;
  }

  // OIDC regstration should be Base64 encoded `ProfileRegistrationPayload`.
  // Halt the operation and log it if any required field is missing, or if error
  // occurs during the parsing.
  if (std::optional<std::string> registration_header =
          headers->GetNormalizedHeader(kRegistrationHeaderField)) {
    std::string decoded_registration_header;
    if (!base::Base64UrlDecode(registration_header.value(),
                               base::Base64UrlDecodePolicy::IGNORE_PADDING,
                               &decoded_registration_header)) {
      VLOG_POLICY(1, OIDC_ENROLLMENT)
          << "Cannot decode value from OIDC registration header field.";
      RecordOidcInterceptionResult(OidcInterceptionResult::kInvalidUrlOrTokens);
      return PROCEED;
    }

    enterprise_management::ProfileRegistrationPayload registration_payload;
    if (!registration_payload.ParseFromString(decoded_registration_header)) {
      VLOG_POLICY(1, OIDC_ENROLLMENT)
          << "Cannot parse OIDC profile registration payload from auth header.";
      RecordOidcInterceptionResult(OidcInterceptionResult::kInvalidUrlOrTokens);
      return PROCEED;
    }

    // Check if any required field of the proto is empty.
    bool is_payload_valid = true;
    for (auto const& key_value : std::map<std::string, std::string>(
             {{kPayloadIssuerFieldName, registration_payload.issuer()},
              {kPayloadSubjectFieldName, registration_payload.subject()},
              {kPayloadCodeFieldName,
               registration_payload.encrypted_user_information()}})) {
      if (key_value.second.empty()) {
        LOG_POLICY(ERROR, OIDC_ENROLLMENT)
            << key_value.first << " has empty value in header payload.";
        is_payload_valid = false;
      }
    }

    if (!is_payload_valid) {
      RecordOidcInterceptionResult(OidcInterceptionResult::kInvalidUrlOrTokens);
      return PROCEED;
    }

    auto* interceptor =
        OidcAuthenticationSigninInterceptorFactory::GetForProfile(profile);

    VLOG_POLICY(2, OIDC_ENROLLMENT)
        << "OIDC header enrollment meets all requirements, starting enrollment "
           "process.";
    RecordOidcInterceptionFunnelStep(
        OidcInterceptionFunnelStep::kSuccessfulInfoParsed);

    // Kick off interceptor with Oidc token containing encrypted user info, the
    // interceptor will automatically choose the correct registration method.
    interception_triggered_ = true;
    interceptor->MaybeInterceptOidcAuthentication(
        navigation_handle()->GetWebContents(),
        ProfileManagementOidcTokens(
            registration_payload.encrypted_user_information()),
        registration_payload.issuer(), registration_payload.subject(),
        registration_payload.email(),
        base::BindOnce(&OidcAuthResponseCaptureNavigationThrottle::Resume,
                       weak_ptr_factory_.GetWeakPtr()));
    return DEFER;
  }

  return PROCEED;
}

}  // namespace profile_management
