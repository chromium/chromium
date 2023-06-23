// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/profile_management/profile_management_navigation_throttle.h"

#include <string>

#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/profile_management/profile_management_features.h"
#include "chrome/browser/enterprise/profile_management/saml_response_parser.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/signin/profile_token_web_signin_interceptor.h"
#include "chrome/browser/signin/profile_token_web_signin_interceptor_factory.h"
#include "components/account_id/account_id.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "net/base/url_util.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace profile_management {

namespace {

constexpr char kGoogleServiceLoginUrl[] =
    "https://www.google.com/a/%s/ServiceLogin";

// Utility struct used to store SAML attributes related to third-party profile
// management.
struct SAMLProfileAttributes {
  std::string name;
  std::string domain;
  std::string token;
};

base::flat_map<std::string, SAMLProfileAttributes>& GetAttributeMap() {
  // TODO(crbug.com/1445072): Add actual domains with claim names.
  static base::NoDestructor<base::flat_map<std::string, SAMLProfileAttributes>>
      profile_attributes(
          {{"supported.test",
            SAMLProfileAttributes("placeholderName", "placeholderDomain",
                                  "placeholderToken")}});
  return *profile_attributes;
}

absl::optional<std::string> GetDomainFromAttributeValue(
    const std::string& domain_attribute_value) {
  // Exclude empty and and dotless domains as they are not supported by the
  // Google identity service.
  if (domain_attribute_value.empty() ||
      domain_attribute_value.find(".") == std::string::npos) {
    return absl::nullopt;
  }

  // If '@' is found in the domain value, treat it as an email address and
  // extract the domain from it.
  if (domain_attribute_value.find("@") != std::string::npos) {
    std::string email_domain = gaia::ExtractDomainName(domain_attribute_value);
    return email_domain.empty() ? absl::nullopt
                                : absl::make_optional(email_domain);
  }

  return domain_attribute_value;
}

// Used to scope posted navigation tasks to the lifetime of `web_contents`.
class ProfileManagementWebContentsLifetimeHelper
    : public content::WebContentsUserData<
          ProfileManagementWebContentsLifetimeHelper> {
 public:
  explicit ProfileManagementWebContentsLifetimeHelper(
      content::WebContents* web_contents)
      : content::WebContentsUserData<
            ProfileManagementWebContentsLifetimeHelper>(*web_contents) {}

  base::WeakPtr<ProfileManagementWebContentsLifetimeHelper> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  void OpenURL(const content::OpenURLParams& url_params) {
    GetWebContents().OpenURL(url_params);
  }

 private:
  friend class content::WebContentsUserData<
      ProfileManagementWebContentsLifetimeHelper>;

  base::WeakPtrFactory<ProfileManagementWebContentsLifetimeHelper>
      weak_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

WEB_CONTENTS_USER_DATA_KEY_IMPL(ProfileManagementWebContentsLifetimeHelper);

}  // namespace

// static
std::unique_ptr<ProfileManagementNavigationThrottle>
ProfileManagementNavigationThrottle::MaybeCreateThrottleFor(
    content::NavigationHandle* navigation_handle) {
  if ((!base::FeatureList::IsEnabled(features::kThirdPartyProfileManagement) &&
       !base::FeatureList::IsEnabled(
           features::kEnableProfileTokenManagement)) ||
      !g_browser_process->local_state() ||
      !profiles::IsProfileCreationAllowed()) {
    return nullptr;
  }

  // The throttle is created for all requests since it intercepts specific HTTP
  // responses.
  return std::make_unique<ProfileManagementNavigationThrottle>(
      navigation_handle);
}

ProfileManagementNavigationThrottle::ProfileManagementNavigationThrottle(
    content::NavigationHandle* navigation_handle)
    : content::NavigationThrottle(navigation_handle) {}

ProfileManagementNavigationThrottle::~ProfileManagementNavigationThrottle() =
    default;

content::NavigationThrottle::ThrottleCheckResult
ProfileManagementNavigationThrottle::WillProcessResponse() {
  if (!base::Contains(GetAttributeMap(),
                      navigation_handle()->GetURL().host())) {
    return PROCEED;
  }

  navigation_handle()->GetResponseBody(
      base::BindOnce(&ProfileManagementNavigationThrottle::OnResponseBodyReady,
                     weak_ptr_factory_.GetWeakPtr()));
  return DEFER;
}

const char* ProfileManagementNavigationThrottle::GetNameForLogging() {
  return "ProfileManagementNavigationThrottle";
}

void ProfileManagementNavigationThrottle::SetURLsForTesting(
    const std::string& token_url,
    const std::string& unmanaged_url) {
  token_url_for_testing_ = token_url;
  unmanaged_url_for_testing_ = unmanaged_url;
}

void ProfileManagementNavigationThrottle::OnResponseBodyReady(
    const std::string& body) {
  // TODO(crbug.com/1445072): As a fallback, check more attributes that may
  // contain the user's email address.
  const auto profile_attributes =
      GetAttributeMap().at(navigation_handle()->GetURL().host());
  saml_response_parser_ = std::make_unique<SAMLResponseParser>(
      std::vector<std::string>{profile_attributes.name,
                               profile_attributes.domain,
                               profile_attributes.token},
      body,
      base::BindOnce(
          &ProfileManagementNavigationThrottle::OnManagementDataReceived,
          weak_ptr_factory_.GetWeakPtr()));
}

void ProfileManagementNavigationThrottle::OnManagementDataReceived(
    const base::flat_map<std::string, std::string>& attributes) {
  const std::string navigation_host = navigation_handle()->GetURL().host();
  DCHECK(base::Contains(GetAttributeMap(), navigation_host));
  const auto profile_attributes = GetAttributeMap().at(navigation_host);

  if (base::FeatureList::IsEnabled(features::kThirdPartyProfileManagement) &&
      base::Contains(attributes, profile_attributes.domain)) {
    RegisterWithDomain(attributes.at(profile_attributes.domain));
    return;
  }

  // If the third-party domain-based profile management feature is disabled, or
  // no domain is found in the response, fall back to token-based management.
  if (base::FeatureList::IsEnabled(features::kEnableProfileTokenManagement) &&
      base::Contains(attributes, profile_attributes.token)) {
    RegisterWithToken(base::Contains(attributes, profile_attributes.name)
                          ? attributes.at(profile_attributes.name)
                          : std::string(),
                      attributes.at(profile_attributes.token));
    return;
  }

  if (!unmanaged_url_for_testing_.empty()) {
    // Used for testing that no profile management flow was entered.
    NavigateTo(GURL(unmanaged_url_for_testing_));
    return;
  }

  Resume();
  // DO NOT ADD CODE AFTER THIS, as the NavigationThrottle might have been
  // deleted by the previous call.
}

void ProfileManagementNavigationThrottle::NavigateTo(const GURL& url) {
  content::WebContents* web_contents = navigation_handle()->GetWebContents();
  if (!web_contents) {
    return;
  }

  ProfileManagementWebContentsLifetimeHelper::CreateForWebContents(
      web_contents);
  ProfileManagementWebContentsLifetimeHelper* helper =
      ProfileManagementWebContentsLifetimeHelper::FromWebContents(web_contents);

  // Post a task to navigate to the desired URL since synchronously starting a
  // navigation from another navigation is an antipattern. This navigation is
  // cancelled immediately after posting the new navigation. The new navigation
  // is scoped to the lifetime of `WebContents`.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &ProfileManagementWebContentsLifetimeHelper::OpenURL,
          helper->GetWeakPtr(),
          content::OpenURLParams(
              url, content::Referrer(), WindowOpenDisposition::CURRENT_TAB,
              ui::PageTransition::PAGE_TRANSITION_CLIENT_REDIRECT,
              /*is_renderer_initiated=*/false)));

  // Only call `CancelDeferredNavigation()` outside of testing since it crashes
  // unit tests.
  if (token_url_for_testing_.empty() && unmanaged_url_for_testing_.empty()) {
    CancelDeferredNavigation(content::NavigationThrottle::CANCEL_AND_IGNORE);
    // DO NOT ADD CODE AFTER THIS, as the NavigationThrottle might have been
    // deleted by the previous call.
  }
}

void ProfileManagementNavigationThrottle::RegisterWithDomain(
    const std::string& domain) {
  absl::optional<std::string> management_domain =
      GetDomainFromAttributeValue(domain);
  if (management_domain) {
    NavigateTo(GURL(base::StringPrintf(kGoogleServiceLoginUrl,
                                       management_domain.value().c_str())));
    return;
  }

  // Only call `Resume()` outside of testing since it crashes unit tests.
  if (token_url_for_testing_.empty() && unmanaged_url_for_testing_.empty()) {
    Resume();
    // DO NOT ADD CODE AFTER THIS, as the NavigationThrottle might have been
    // deleted by the previous call.
  }
}

void ProfileManagementNavigationThrottle::RegisterWithToken(
    const std::string& name,
    const std::string& token) {
  if (!token_url_for_testing_.empty()) {
    // Used for testing that the token-based management flow was entered.
    NavigateTo(GURL(token_url_for_testing_));
    return;
  }

  auto* interceptor = ProfileTokenWebSigninInterceptorFactory::GetForProfile(
      Profile::FromBrowserContext(
          navigation_handle()->GetWebContents()->GetBrowserContext()));
  interceptor->MaybeInterceptSigninProfile(
      navigation_handle()->GetWebContents(), name, token);

  Resume();
  // DO NOT ADD CODE AFTER THIS, as the NavigationThrottle might have been
  // deleted by the previous call.
}

}  // namespace profile_management
