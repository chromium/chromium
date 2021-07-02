// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/https_only_mode_navigation_throttle.h"

#include "base/feature_list.h"
#include "base/task/post_task.h"
#include "base/time/time.h"
#include "chrome/browser/ssl/https_only_mode_tab_storage.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

namespace {

base::TimeDelta g_fallback_delay = base::TimeDelta::FromSeconds(3);

}  // namespace

// static
std::unique_ptr<HttpsOnlyModeNavigationThrottle>
HttpsOnlyModeNavigationThrottle::MaybeCreateThrottleFor(
    content::NavigationHandle* handle,
    PrefService* prefs) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // HTTPS-Only Mode is only relevant for primary main-frame HTTP(S)
  // navigations.
  if (!handle->GetURL().SchemeIsHTTPOrHTTPS() ||
      !handle->IsInPrimaryMainFrame() || handle->IsSameDocument()) {
    return nullptr;
  }

  if (!base::FeatureList::IsEnabled(features::kHttpsOnlyMode) || !prefs ||
      !prefs->GetBoolean(prefs::kHttpsOnlyModeEnabled)) {
    return nullptr;
  }

  return std::make_unique<HttpsOnlyModeNavigationThrottle>(handle);
}

HttpsOnlyModeNavigationThrottle::HttpsOnlyModeNavigationThrottle(
    content::NavigationHandle* handle)
    : content::NavigationThrottle(handle) {}

HttpsOnlyModeNavigationThrottle::~HttpsOnlyModeNavigationThrottle() = default;

content::NavigationThrottle::ThrottleCheckResult
HttpsOnlyModeNavigationThrottle::WillStartRequest() {
  // If the navigation was upgraded by the Interceptor, start the timeout timer.
  // Which navigations to upgrade is determined by the Interceptor not the
  // Throttle.
  auto* tab_storage = HttpsOnlyModeTabStorage::GetOrCreate(
      navigation_handle()->GetWebContents());
  if (tab_storage->is_navigation_upgraded()) {
    timer_.Start(FROM_HERE, g_fallback_delay, this,
                 &HttpsOnlyModeNavigationThrottle::OnHttpsLoadTimeout);
  }

  return content::NavigationThrottle::PROCEED;
}

// Called if there is a non-OK net::Error in the completion status.
content::NavigationThrottle::ThrottleCheckResult
HttpsOnlyModeNavigationThrottle::WillFailRequest() {
  // Cancel the request, stop the timer, and trigger the HTTPS-Only Mode
  // interstitial in case of SSL errors or other net errors.
  timer_.Stop();

  // If there was no certificate error, SSLInfo will be empty.
  const net::SSLInfo info =
      navigation_handle()->GetSSLInfo().value_or(net::SSLInfo());
  int cert_status = info.cert_status;
  if (!net::IsCertStatusError(cert_status) &&
      navigation_handle()->GetNetErrorCode() == net::OK) {
    // Don't fallback.
    return content::NavigationThrottle::PROCEED;
  }

  // Only show the interstitial if the Interceptor attempted to upgrade the
  // navigation.
  auto* tab_storage = HttpsOnlyModeTabStorage::GetOrCreate(
      navigation_handle()->GetWebContents());
  if (tab_storage->is_navigation_upgraded()) {
    // For now this just adds the host to a basic tab-scoped allowlist and shows
    // a placeholder error string.
    // TODO(crbug.com/1218526): Replace this placeholder with the
    // actual blocking page HTML.
    tab_storage->AddHostToAllowlist(navigation_handle()->GetURL().host());
    return content::NavigationThrottle::ThrottleCheckResult(
        content::NavigationThrottle::CANCEL, net::ERR_BLOCKED_BY_CLIENT,
        std::string(
            "<html><body>Blocked due to HTTPS-Only Mode</body></html>"));
  }

  return content::NavigationThrottle::PROCEED;
}

content::NavigationThrottle::ThrottleCheckResult
HttpsOnlyModeNavigationThrottle::WillRedirectRequest() {
  // HTTPS->HTTP downgrades may result in net::ERR_TOO_MANY_REDIRECTS, but these
  // redirect loops should hit the cache and not cost too much. If they go too
  // long, the fallback timer will kick in. ERR_TOO_MANY_REDIRECTS should result
  // in the request failing and triggering fallback.
  //
  // Alternately, the Interceptor could log URLs seen and bail if it encounters
  // a redirect loop, but it is simpler to rely on existing handling unless
  // the optimization is needed.

  // If the timer is not yet started and this is now an upgraded navigation,
  // then start the timer here. This can happen if the initial request is to
  // HTTPS but then redirects to HTTP.
  auto* tab_storage = HttpsOnlyModeTabStorage::GetOrCreate(
      navigation_handle()->GetWebContents());
  if (tab_storage->is_navigation_upgraded() && !timer_.IsRunning()) {
    timer_.Start(FROM_HERE, g_fallback_delay, this,
                 &HttpsOnlyModeNavigationThrottle::OnHttpsLoadTimeout);
  }

  return content::NavigationThrottle::PROCEED;
}

content::NavigationThrottle::ThrottleCheckResult
HttpsOnlyModeNavigationThrottle::WillProcessResponse() {
  // The HTTPS load succeeded. Stop the timer.
  timer_.Stop();

  // Clear the status for this navigation as it will successfully commit.
  auto* tab_storage = HttpsOnlyModeTabStorage::GetOrCreate(
      navigation_handle()->GetWebContents());
  tab_storage->set_is_navigation_upgraded(false);

  return content::NavigationThrottle::PROCEED;
}

const char* HttpsOnlyModeNavigationThrottle::GetNameForLogging() {
  return "HttpsOnlyModeNavigationThrottle";
}

// static
void HttpsOnlyModeNavigationThrottle::set_timeout_for_testing(
    int timeout_in_seconds) {
  g_fallback_delay = base::TimeDelta::FromSeconds(timeout_in_seconds);
}

void HttpsOnlyModeNavigationThrottle::OnHttpsLoadTimeout() {
  // Stop the current navigation and replace it with an error page.
  auto* web_contents = navigation_handle()->GetWebContents();
  auto* tab_storage = HttpsOnlyModeTabStorage::GetOrCreate(web_contents);
  tab_storage->set_is_navigation_upgraded(false);

  // For now this just adds the host to a basic tab-scoped allowlist and shows
  // a placeholder error string.
  // TODO(crbug.com/crbug.com/1218526): Replace this placeholder with the
  // actual blocking page HTML.
  tab_storage->AddHostToAllowlist(navigation_handle()->GetURL().host());
  web_contents->GetController().LoadPostCommitErrorPage(
      web_contents->GetMainFrame(), navigation_handle()->GetURL(),
      std::string("<html><body>Blocked due to HTTPS-Only Mode</body></html>"),
      net::ERR_BLOCKED_BY_CLIENT);
}
