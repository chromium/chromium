// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/platform_auth/platform_auth_navigation_throttle.h"

#include "base/feature_list.h"
#include "build/buildflag.h"
#include "chrome/browser/enterprise/platform_auth/platform_auth_features.h"
#include "chrome/browser/enterprise/platform_auth/platform_auth_provider_manager.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "net/cookies/cookie_util.h"
#include "net/http/http_request_headers.h"

namespace enterprise_auth {
namespace {

#if BUILDFLAG(IS_MAC)
// This is for testing purposes. At the moment we have to pretend to be Safari
// while requesting resources from okta.com domain. Eventually, before the
// release and once Okta implements the change on their side, this will become
// obsolete and can be removed.
void SpoofUserAgent(content::NavigationHandle* navigation_handle) {
  navigation_handle->SetRequestHeader(
      "User-Agent",
      "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_14_6) "
      "AppleWebKit/605.1.15 "
      "(KHTML, like Gecko) Version/13.0.3 Safari/605.1.15");
}

constexpr char kOktaDomain[] = "okta.com";
#endif

}  // namespace

// static
void PlatformAuthNavigationThrottle::MaybeCreateAndAdd(
    content::NavigationThrottleRegistry& registry) {
  // The manager is enabled when both the feature and policy are enabled.
  // If the manager is not enabled, there is no point in creating a throttle
  // since no auth data can be fetched.
  if (!PlatformAuthProviderManager::GetInstance().IsEnabled()) {
    return;
  }

  // To ensure that auth data is attached to both requests and redirects, the
  // navigation throttle is created for all requests.
  registry.AddThrottle(
      std::make_unique<PlatformAuthNavigationThrottle>(registry));
}

PlatformAuthNavigationThrottle::PlatformAuthNavigationThrottle(
    content::NavigationThrottleRegistry& registry)
    : content::NavigationThrottle(registry) {}

PlatformAuthNavigationThrottle::~PlatformAuthNavigationThrottle() = default;

content::NavigationThrottle::ThrottleCheckResult
PlatformAuthNavigationThrottle::WillStartRequest() {
#if BUILDFLAG(IS_MAC)
  // TODO: crbug.com/461709143 - Cleanup user agent spoofing when starting a
  // request.
  if (base::FeatureList::IsEnabled(enterprise_auth::kOktaSSO) &&
      navigation_handle()->GetURL().DomainIs(kOktaDomain)) {
    SpoofUserAgent(navigation_handle());
  }
#endif

  // The manager is enabled when both the feature and policy are enabled. This
  // value is set in `ResourceRequest::TrustedParams`, which can only be
  // modified at the start of a request (not during redirects).
  navigation_handle()->SetAllowCookiesFromBrowser(
      PlatformAuthProviderManager::GetInstance().IsEnabled());
  return FetchHeaders();
}

content::NavigationThrottle::ThrottleCheckResult
PlatformAuthNavigationThrottle::WillRedirectRequest() {
#if BUILDFLAG(IS_MAC)
  // TODO: crbug.com/461709143 - Cleanup user agent spoofing when redirecting a
  // request.
  if (base::FeatureList::IsEnabled(enterprise_auth::kOktaSSO) &&
      navigation_handle()->GetURL().DomainIs(kOktaDomain)) {
    SpoofUserAgent(navigation_handle());
  }
#endif

  for (auto header : attached_headers_) {
    navigation_handle()->RemoveRequestHeader(header);
  }

  attached_headers_.clear();
  return FetchHeaders();
}

const char* PlatformAuthNavigationThrottle::GetNameForLogging() {
  return "PlatformAuthNavigationThrottle";
}

content::NavigationThrottle::ThrottleCheckResult
PlatformAuthNavigationThrottle::FetchHeaders() {
  fetch_headers_callback_ran_ = false;
  PlatformAuthProviderManager::GetInstance().GetData(
      navigation_handle()->GetURL(),
      base::BindOnce(&PlatformAuthNavigationThrottle::FetchHeadersCallback,
                     weak_ptr_factory_.GetWeakPtr()));

  // If the header fetch callback already ran it likely means that headers could
  // not be fetched or the auth manager is not enabled for the current URL. In
  // either case,`PlatformAuthProviderManager::GetData()` returned
  // synchronously, so no need to defer.
  if (fetch_headers_callback_ran_) {
    return content::NavigationThrottle::PROCEED;
  }

  is_deferred_ = true;
  return content::NavigationThrottle::DEFER;
}

void PlatformAuthNavigationThrottle::FetchHeadersCallback(
    net::HttpRequestHeaders auth_headers) {
  DCHECK(attached_headers_.empty());
  attached_headers_.reserve(auth_headers.GetHeaderVector().size());
  net::HttpRequestHeaders::Iterator it(auth_headers);
  while (it.GetNext()) {
    attached_headers_.push_back(it.name());
    navigation_handle()->SetRequestHeader(it.name(), it.value());
  }
  fetch_headers_callback_ran_ = true;

  // Resume the deferred request.
  if (is_deferred_) {
    is_deferred_ = false;
    // `Resume()` can synchronously delete this navigation throttle, so no code
    // after it should reference a throttle instance.
    Resume();
  }
}

}  // namespace enterprise_auth
