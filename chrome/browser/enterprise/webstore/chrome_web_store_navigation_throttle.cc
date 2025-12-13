// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/webstore/chrome_web_store_navigation_throttle.h"

#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "components/enterprise/browser/controller/browser_dm_token_storage.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/extension_urls.h"
#include "net/http/http_request_headers.h"

namespace enterprise_webstore {

char kDeviceIdHeader[] = "X-Client-Device-Id";
char kDmTokenHeader[] = "X-Browser-Dm-Token";

ChromeWebStoreNavigationThrottle::ChromeWebStoreNavigationThrottle(
    content::NavigationThrottleRegistry& registry)
    : content::NavigationThrottle(registry) {}

ChromeWebStoreNavigationThrottle::~ChromeWebStoreNavigationThrottle() = default;

content::NavigationThrottle::ThrottleCheckResult
ChromeWebStoreNavigationThrottle::WillStartRequest() {
  return MaybeAppendHeaders();
}

content::NavigationThrottle::ThrottleCheckResult
ChromeWebStoreNavigationThrottle::WillRedirectRequest() {
  return MaybeAppendHeaders();
}

content::NavigationThrottle::ThrottleCheckResult
ChromeWebStoreNavigationThrottle::MaybeAppendHeaders() {
  if (!navigation_handle()->GetURL().DomainIs(
          extension_urls::GetNewWebstoreLaunchURL().GetHost())) {
    return content::NavigationThrottle::PROCEED;
  }

  content::WebContents* web_contents = navigation_handle()->GetWebContents();
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  if (!profile->IsRegularProfile()) {
    return content::NavigationThrottle::PROCEED;
  }

  std::optional<policy::DMToken> optional_dm_token = policy::GetDMToken();
  if (!optional_dm_token || !optional_dm_token->is_valid()) {
    return content::NavigationThrottle::PROCEED;
  }

  navigation_handle()->SetRequestHeader(kDmTokenHeader,
                                        optional_dm_token->value());
  navigation_handle()->SetRequestHeader(
      kDeviceIdHeader,
      policy::BrowserDMTokenStorage::Get()->RetrieveClientId());
  return content::NavigationThrottle::PROCEED;
}

const char* ChromeWebStoreNavigationThrottle::GetNameForLogging() {
  return "ChromeWebStoreNavigationThrottle";
}
}  // namespace enterprise_webstore
