// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/platform_auth/platform_auth_navigation_throttle.h"

#include "chrome/browser/enterprise/platform_auth/platform_auth_provider_manager.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "net/cookies/cookie_util.h"
#include "net/http/http_request_headers.h"

namespace enterprise_auth {
// static
std::unique_ptr<PlatformAuthNavigationThrottle>
PlatformAuthNavigationThrottle::MaybeCreateThrottleFor(
    content::NavigationHandle* navigation_handle) {
  // The manager is enabled when both the feature and policy are enabled.
  // If the manager is not enabled, there is no point in creating a throttle
  // since no auth data can be fetched.
  if (!PlatformAuthProviderManager::GetInstance().IsEnabled())
    return nullptr;

  // To ensure that auth data is attached to both requests and redirects, the
  // navigation throttle is created for all requests.
  return std::make_unique<PlatformAuthNavigationThrottle>(navigation_handle);
}

PlatformAuthNavigationThrottle::PlatformAuthNavigationThrottle(
    content::NavigationHandle* navigation_handle)
    : content::NavigationThrottle(navigation_handle) {}

PlatformAuthNavigationThrottle::~PlatformAuthNavigationThrottle() = default;

content::NavigationThrottle::ThrottleCheckResult
PlatformAuthNavigationThrottle::WillStartRequest() {
  // The manager is enabled when both the feature and policy are enabled. This
  // value is set in `ResourceRequest::TrustedParams`, which can only be
  // modified at the start of a request (not during redirects).
  navigation_handle()->SetAllowCookiesFromBrowser(
      PlatformAuthProviderManager::GetInstance().IsEnabled());
  return FetchHeaders();
}

content::NavigationThrottle::ThrottleCheckResult
PlatformAuthNavigationThrottle::WillRedirectRequest() {
  for (auto header : attached_headers_)
    navigation_handle()->RemoveRequestHeader(header);

  attached_headers_.clear();
  return FetchHeaders();
}

const char* PlatformAuthNavigationThrottle::GetNameForLogging() {
  return "PlatformAuthNavigationThrottle";
}

content::NavigationThrottle::ThrottleCheckResult
PlatformAuthNavigationThrottle::FetchHeaders() {
  fetch_headers_callback_ran_ = false;

  // `PlatformAuthProviderManager` may be in the middle of an asynchronous state
  // change, such as becoming disabled or updating its supported IdP origins, in
  // which case the auth data fetch may still succeed.
  if (!PlatformAuthProviderManager::GetInstance().IsEnabledFor(
          navigation_handle()->GetURL()))
    return content::NavigationThrottle::PROCEED;

  PlatformAuthProviderManager::GetInstance().GetData(
      navigation_handle()->GetURL(),
      base::BindOnce(&PlatformAuthNavigationThrottle::FetchHeadersCallback,
                     weak_ptr_factory_.GetWeakPtr()));

  // If the header fetch callback already ran it likely means that headers could
  // not be fetched and `PlatformAuthProviderManager::GetData()` returned
  // synchronously, so no need to defer.
  if (fetch_headers_callback_ran_)
    return content::NavigationThrottle::PROCEED;

  is_deferred_ = true;
  return content::NavigationThrottle::DEFER;
}

void PlatformAuthNavigationThrottle::FetchHeadersCallback(
    net::HttpRequestHeaders auth_headers) {
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
