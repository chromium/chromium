// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/profile_management/profile_management_navigation_throttle.h"

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/profile_management/profile_management_features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/signin/profile_token_web_signin_interceptor.h"
#include "chrome/browser/signin/profile_token_web_signin_interceptor_factory.h"
#include "components/account_id/account_id.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "net/base/url_util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace profile_management {

constexpr char kTestHost[] = "www.google.com";

namespace {

constexpr std::array<const char*, 1> kSupportedHosts{
    kTestHost,
};

class QueryParamTokenInfoGetter
    : public ProfileManagementNavigationThrottle::TokenInfoGetter {
 public:
  QueryParamTokenInfoGetter() = default;
  ~QueryParamTokenInfoGetter() override = default;

  void GetTokenInfo(
      content::NavigationHandle* navigation_handle,
      base::OnceCallback<void(const std::string&, const std::string&)> callback)
      override {
    auto url = navigation_handle->GetURL();
    DCHECK_EQ(url.host(), kTestHost);
    std::string id;
    std::string token;
    net::GetValueForKeyInQuery(url, "id", &id);
    net::GetValueForKeyInQuery(url, "token", &token);
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), id, token));
  }
};

}  // namespace

ProfileManagementNavigationThrottle::TokenInfoGetter::~TokenInfoGetter() =
    default;

// static
std::unique_ptr<ProfileManagementNavigationThrottle>
ProfileManagementNavigationThrottle::MaybeCreateThrottleFor(
    content::NavigationHandle* navigation_handle) {
  if (!base::FeatureList::IsEnabled(features::kEnableProfileTokenManagement) ||
      !g_browser_process->local_state() ||
      !profiles::IsProfileCreationAllowed()) {
    return nullptr;
  }

  return std::make_unique<ProfileManagementNavigationThrottle>(
      navigation_handle, std::make_unique<QueryParamTokenInfoGetter>());
}

ProfileManagementNavigationThrottle::ProfileManagementNavigationThrottle(
    content::NavigationHandle* navigation_handle,
    std::unique_ptr<TokenInfoGetter> token_info_getter)
    : NavigationThrottle(navigation_handle),
      token_info_getter_(std::move(token_info_getter)) {
  DCHECK(token_info_getter_);
}

ProfileManagementNavigationThrottle::~ProfileManagementNavigationThrottle() =
    default;

content::NavigationThrottle::ThrottleCheckResult
ProfileManagementNavigationThrottle::WillProcessResponse() {
  auto host = navigation_handle()->GetURL().host();
  if (base::Contains(kSupportedHosts, host)) {
    token_info_getter_->GetTokenInfo(
        navigation_handle(),
        base::BindOnce(
            &ProfileManagementNavigationThrottle::OnTokenInfoReceived,
            weak_ptr_factory_.GetWeakPtr()));
    return DEFER;
  }
  return PROCEED;
}

const char* ProfileManagementNavigationThrottle::GetNameForLogging() {
  return "ProfileManagementNavigationThrottle";
}

void ProfileManagementNavigationThrottle::OnTokenInfoReceived(
    const std::string& id,
    const std::string& management_token) {
  if (!management_token.empty()) {
    auto* interceptor = ProfileTokenWebSigninInterceptorFactory::GetForProfile(
        Profile::FromBrowserContext(
            navigation_handle()->GetWebContents()->GetBrowserContext()));
    interceptor->MaybeInterceptSigninProfile(
        navigation_handle()->GetWebContents(), id, management_token);
  }

  Resume();
}

}  // namespace profile_management
