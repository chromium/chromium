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

// Chrome Enterprise page that handles OIDC authentication redirection, this
// page should receive the proper payload in its auth header to start OIDC
// profile creation/registration.
constexpr char kEnterpriseOidcRegisterUrl[] =
    "https://chromeenterprise.google/profile-enrollment/register-handler";

constexpr char kRegistrationHeaderField[] = "X-Profile-Registration-Payload";

constexpr char kPayloadIssuerFieldName[] = "issuer";
constexpr char kPayloadSubjectFieldName[] = "subject";
constexpr char kPayloadCodeFieldName[] = "encrypted_user_information";

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
OidcAuthResponseCaptureNavigationThrottle::WillProcessResponse() {
  if (base::FeatureList::IsEnabled(
          profile_management::features::kOidcAuthHeaderInterception)) {
    return AttemptToTriggerHeaderInterception();
  }

  return PROCEED;
}

const char* OidcAuthResponseCaptureNavigationThrottle::GetNameForLogging() {
  return "OidcAuthResponseCaptureNavigationThrottle";
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
