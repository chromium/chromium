// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/https_upgrades_navigation_throttle.h"

#include "base/feature_list.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/https_only_mode_tab_helper.h"
#include "chrome/browser/ssl/https_upgrades_navigation_throttle.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "components/security_interstitials/content/stateful_ssl_host_state_delegate.h"
#include "components/security_interstitials/core/https_only_mode_metrics.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/web_contents.h"
#include "net/base/net_errors.h"

using security_interstitials::https_only_mode::Event;

namespace {

// Time that the throttle will wait before canceling the upgraded navigation and
// showing the HTTPS-First Mode interstitial.
base::TimeDelta g_fallback_delay = base::Seconds(3);

// Helper to record an HTTPS-First Mode navigation event.
// TODO(crbug.com/1394910): Rename these metrics now that they apply to both
// HTTPS-First Mode and HTTPS Upgrades.
void RecordHttpsFirstModeNavigation(
    security_interstitials::https_only_mode::Event event) {
  base::UmaHistogramEnumeration(
      security_interstitials::https_only_mode::kEventHistogram, event);
}

}  // namespace

// static
std::unique_ptr<HttpsUpgradesNavigationThrottle>
HttpsUpgradesNavigationThrottle::MaybeCreateThrottleFor(
    content::NavigationHandle* handle,
    std::unique_ptr<SecurityBlockingPageFactory> blocking_page_factory,
    PrefService* prefs) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // HTTPS-First Mode is only relevant for primary main-frame HTTP(S)
  // navigations.
  if (!handle->GetURL().SchemeIsHTTPOrHTTPS() ||
      !handle->IsInPrimaryMainFrame() || handle->IsSameDocument()) {
    return nullptr;
  }

  bool https_first_mode_enabled =
      base::FeatureList::IsEnabled(features::kHttpsFirstModeV2) && prefs &&
      prefs->GetBoolean(prefs::kHttpsOnlyModeEnabled);
  bool https_upgrades_enabled =
      https_first_mode_enabled ||
      base::FeatureList::IsEnabled(features::kHttpsUpgrades);
  if (!https_upgrades_enabled) {
    return nullptr;
  }

  // Ensure that the HttpsOnlyModeTabHelper has been created (this does nothing
  // if it has already been created for the WebContents). There are cases where
  // the tab helper won't get created by the initialization in
  // chrome/browser/ui/tab_helpers.cc but the criteria for adding the throttle
  // are still met (see crbug.com/1233889 for one example).
  HttpsOnlyModeTabHelper::CreateForWebContents(handle->GetWebContents());

  return std::make_unique<HttpsUpgradesNavigationThrottle>(
      handle, std::move(blocking_page_factory), https_first_mode_enabled);
}

HttpsUpgradesNavigationThrottle::HttpsUpgradesNavigationThrottle(
    content::NavigationHandle* handle,
    std::unique_ptr<SecurityBlockingPageFactory> blocking_page_factory,
    bool http_interstitial_enabled)
    : content::NavigationThrottle(handle),
      blocking_page_factory_(std::move(blocking_page_factory)),
      http_interstitial_enabled_(http_interstitial_enabled) {}

HttpsUpgradesNavigationThrottle::~HttpsUpgradesNavigationThrottle() = default;

content::NavigationThrottle::ThrottleCheckResult
HttpsUpgradesNavigationThrottle::WillStartRequest() {
  // If the navigation is fallback to HTTP, trigger the HTTP interstitial (if
  // enabled).
  auto* handle = navigation_handle();
  auto* contents = handle->GetWebContents();
  auto* tab_helper = HttpsOnlyModeTabHelper::FromWebContents(contents);
  if (tab_helper->is_navigation_fallback() &&
      !handle->GetURL().SchemeIsCryptographic() && http_interstitial_enabled_) {
    std::unique_ptr<security_interstitials::HttpsOnlyModeBlockingPage>
        blocking_page = blocking_page_factory_->CreateHttpsOnlyModeBlockingPage(
            contents, handle->GetURL());
    std::string interstitial_html = blocking_page->GetHTMLContents();
    security_interstitials::SecurityInterstitialTabHelper::
        AssociateBlockingPage(handle, std::move(blocking_page));
    return content::NavigationThrottle::ThrottleCheckResult(
        content::NavigationThrottle::CANCEL, net::ERR_BLOCKED_BY_CLIENT,
        interstitial_html);
  }

  // Navigation is HTTPS or an initial HTTP navigation (which will get
  // upgraded by the interceptor).
  return content::NavigationThrottle::ThrottleAction::PROCEED;
}

// Called if there is a non-OK net::Error in the completion status.
content::NavigationThrottle::ThrottleCheckResult
HttpsUpgradesNavigationThrottle::WillFailRequest() {
  auto* handle = navigation_handle();

  // If there was no certificate error, SSLInfo will be empty.
  const net::SSLInfo info = handle->GetSSLInfo().value_or(net::SSLInfo());
  int cert_status = info.cert_status;
  if (!net::IsCertStatusError(cert_status) &&
      handle->GetNetErrorCode() == net::OK) {
    // Don't fallback.
    return content::NavigationThrottle::PROCEED;
  }

  // Only fallback to HTTP if the Interceptor attempted to upgrade the
  // navigation.
  auto* contents = handle->GetWebContents();
  auto* tab_helper = HttpsOnlyModeTabHelper::FromWebContents(contents);
  if (tab_helper->is_navigation_upgraded()) {
    // Record failure type metrics for upgraded navigations.
    RecordHttpsFirstModeNavigation(Event::kUpgradeFailed);
    if (net::IsCertificateError(handle->GetNetErrorCode())) {
      RecordHttpsFirstModeNavigation(Event::kUpgradeCertError);
    } else if (handle->GetNetErrorCode() == net::ERR_TIMED_OUT) {
      RecordHttpsFirstModeNavigation(Event::kUpgradeTimedOut);
    } else {
      RecordHttpsFirstModeNavigation(Event::kUpgradeNetError);
    }

    // If HTTPS-First Mode is not enabled (so no interstitial will be shown),
    // add the hostname to the allowlist now before triggering fallback.
    // HTTPS-First Mode handles this on the user proceeding through the
    // interstitial only.
    if (!http_interstitial_enabled_) {
      Profile* profile =
          Profile::FromBrowserContext(contents->GetBrowserContext());
      StatefulSSLHostStateDelegate* state =
          static_cast<StatefulSSLHostStateDelegate*>(
              profile->GetSSLHostStateDelegate());
      // StatefulSSLHostStateDelegate can be null during tests.
      if (state) {
        state->AllowHttpForHost(
            handle->GetURL().host(),
            contents->GetPrimaryMainFrame()->GetStoragePartition());
      }
      tab_helper->set_is_navigation_upgraded(false);
    }

    // Mark the navigation as fallback and trigger a new navigation to the
    // fallback URL.
    tab_helper->set_is_navigation_fallback(true);

    // Copy the original navigation's params to the extent possible but update
    // the URL to navigate to the fallback HTTP URL.
    content::OpenURLParams params =
        content::OpenURLParams::FromNavigationHandle(handle);
    params.url = tab_helper->fallback_url();
    // Post a task to navigate to the fallback URL. We don't navigate
    // synchronously here, as starting a navigation within a navigation is an
    // antipattern.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(
                       [](base::WeakPtr<content::WebContents> web_contents,
                          const content::OpenURLParams& url_params) {
                         if (!web_contents) {
                           return;
                         }
                         web_contents->OpenURL(url_params);
                       },
                       contents->GetWeakPtr(), std::move(params)));
    return content::NavigationThrottle::CANCEL_AND_IGNORE;
  }

  return content::NavigationThrottle::PROCEED;
}

content::NavigationThrottle::ThrottleCheckResult
HttpsUpgradesNavigationThrottle::WillRedirectRequest() {
  // If the navigation was upgraded by the Interceptor, then the Throttle's
  // WillRedirectRequest() will get triggered by the artificial redirect to
  // HTTPS. The HTTPS upgrade will always happen after the Throttle's
  // WillStartRequest() (which only checks for fallback HTTP), so tracking
  // upgraded requests is deferred to WillRedirectRequest() here. Which
  // navigations to upgrade is determined by the Interceptor, not the Throttle.
  //
  // The navigation may get upgraded at various points during redirects:
  //   1. The Interceptor serves an artificial redirect to HTTPS if the
  //      navigation is upgraded. This means the Throttle will see the upgraded
  //      navigation state for the first time here in WillRedirectRequest().
  //   2. HTTPS->HTTP downgrades can occur later in the lifecycle of a
  //      navigation, and will also result in the Interceptor serving an
  //      artificial redirect to upgrade the navigation.
  //
  // HTTPS->HTTP downgrades may result in net::ERR_TOO_MANY_REDIRECTS, but these
  // redirect loops should hit the cache and not cost too much. If they go too
  // long, the fallback timer will kick in. ERR_TOO_MANY_REDIRECTS should result
  // in the request failing and triggering fallback. Alternately, the
  // Interceptor could log URLs seen and bail if it encounters a redirect loop,
  // but it is simpler to rely on existing handling unless the optimization is
  // needed.
  auto* tab_helper = HttpsOnlyModeTabHelper::FromWebContents(
      navigation_handle()->GetWebContents());
  if (tab_helper->is_navigation_upgraded()) {
    // Check if the timer is already started, as there may be additional
    // redirects on the navigation after the artificial upgrade redirect.
    bool timer_started =
        navigation_handle()->SetNavigationTimeout(g_fallback_delay);
    if (timer_started) {
      RecordHttpsFirstModeNavigation(Event::kUpgradeAttempted);
    }
  }

  return content::NavigationThrottle::PROCEED;
}

content::NavigationThrottle::ThrottleCheckResult
HttpsUpgradesNavigationThrottle::WillProcessResponse() {
  // Clear the status for this navigation as it will successfully commit.
  auto* tab_helper = HttpsOnlyModeTabHelper::FromWebContents(
      navigation_handle()->GetWebContents());
  if (tab_helper->is_navigation_upgraded()) {
    RecordHttpsFirstModeNavigation(Event::kUpgradeSucceeded);
    tab_helper->set_is_navigation_upgraded(false);
  }

  // Clear the fallback flag, if set.
  tab_helper->set_is_navigation_fallback(false);

  return content::NavigationThrottle::PROCEED;
}

const char* HttpsUpgradesNavigationThrottle::GetNameForLogging() {
  return "HttpsUpgradesNavigationThrottle";
}

// static
void HttpsUpgradesNavigationThrottle::set_timeout_for_testing(
    int timeout_in_seconds) {
  g_fallback_delay = base::Seconds(timeout_in_seconds);
}
