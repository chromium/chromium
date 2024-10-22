// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/https_upgrades_navigation_throttle.h"

#include "base/feature_list.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/https_first_mode_settings_tracker.h"
#include "chrome/browser/ssl/https_only_mode_tab_helper.h"
#include "chrome/browser/ssl/https_upgrades_navigation_throttle.h"
#include "chrome/browser/ssl/https_upgrades_util.h"
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
#include "ui/base/page_transition_types.h"

using security_interstitials::https_only_mode::Event;

namespace {

// Time that the throttle will wait before canceling the upgraded navigation and
// showing the HTTPS-First Mode interstitial.
base::TimeDelta g_fallback_delay = base::Seconds(3);

}  // namespace

// static
std::unique_ptr<HttpsUpgradesNavigationThrottle>
HttpsUpgradesNavigationThrottle::MaybeCreateThrottleFor(
    content::NavigationHandle* handle,
    std::unique_ptr<SecurityBlockingPageFactory> blocking_page_factory,
    Profile* profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // HTTPS-First Mode is only relevant for primary main-frame HTTP(S)
  // navigations.
  if (!handle->GetURL().SchemeIsHTTPOrHTTPS() ||
      !handle->IsInPrimaryMainFrame() || handle->IsSameDocument()) {
    return nullptr;
  }

  PrefService* prefs = profile->GetPrefs();
  security_interstitials::https_only_mode::HttpInterstitialState
      interstitial_state;
  interstitial_state.enabled_by_pref =
      prefs && prefs->GetBoolean(prefs::kHttpsOnlyModeEnabled);

  if (base::FeatureList::IsEnabled(features::kHttpsFirstModeIncognito)) {
    if (profile->IsIncognitoProfile() && prefs &&
        prefs->GetBoolean(prefs::kHttpsFirstModeIncognito)) {
      interstitial_state.enabled_by_incognito = true;
    }
  }

  StatefulSSLHostStateDelegate* state =
      static_cast<StatefulSSLHostStateDelegate*>(
          profile->GetSSLHostStateDelegate());

  if (IsBalancedModeEnabled(prefs) && state &&
      !state->HttpsFirstBalancedModeSuppressedForTesting()) {
    interstitial_state.enabled_in_balanced_mode = true;
  }

  auto* storage_partition =
      handle->GetWebContents()->GetPrimaryMainFrame()->GetStoragePartition();

  HttpsFirstModeService* hfm_service =
      HttpsFirstModeServiceFactory::GetForProfile(profile);
  if (hfm_service) {
    // Can be null in some cases, e.g. when using Ash sign-in profile.
    hfm_service->IncrementRecentNavigationCount();
    interstitial_state.enabled_by_typically_secure_browsing =
        hfm_service->IsInterstitialEnabledByTypicallySecureUserHeuristic();
  }

  // StatefulSSLHostStateDelegate can be null during tests.
  if (state &&
      state->IsHttpsEnforcedForUrl(handle->GetURL(), storage_partition)) {
    interstitial_state.enabled_by_engagement_heuristic = true;
  }

  bool https_upgrades_enabled =
      interstitial_state.enabled_by_pref ||
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
      handle, profile, std::move(blocking_page_factory), interstitial_state);
}

HttpsUpgradesNavigationThrottle::HttpsUpgradesNavigationThrottle(
    content::NavigationHandle* handle,
    Profile* profile,
    std::unique_ptr<SecurityBlockingPageFactory> blocking_page_factory,
    security_interstitials::https_only_mode::HttpInterstitialState
        interstitial_state)
    : content::NavigationThrottle(handle),
      profile_(profile),
      blocking_page_factory_(std::move(blocking_page_factory)),
      interstitial_state_(interstitial_state) {}

HttpsUpgradesNavigationThrottle::~HttpsUpgradesNavigationThrottle() = default;

content::NavigationThrottle::ThrottleCheckResult
HttpsUpgradesNavigationThrottle::WillStartRequest() {
  // If the navigation is fallback to HTTP, trigger the HTTP interstitial (if
  // enabled). The interceptor creates a redirect for the fallback navigation,
  // which will trigger MaybeCreateLoader() in the interceptor for the redirect
  // but *doesn't* trigger WillStartRequest() because it's all part of the same
  // request. Here, we skip directly to showing the HTTP interstitial if this
  // is:
  //   (1) a back/forward navigation, and
  //   (2) the URL already failed upgrades before.
  // This lets us avoid triggering the Interceptor during a back/forward
  // navigation (which breaks history state) and acts like the browser
  // "remembering" the state of the tab as being on the interstitial for that
  // URL.
  //
  // Other cases for starting a navigation to a URL that previously failed
  // to be upgraded should go through the full upgrade flow -- better to assume
  // that something may have changed in the time since. For example: a user
  // reloading the tab showing the interstitial should re-try the upgrade.
  auto* handle = navigation_handle();
  auto* contents = handle->GetWebContents();
  auto* tab_helper = HttpsOnlyModeTabHelper::FromWebContents(contents);

  if ((handle->GetPageTransition() & ui::PAGE_TRANSITION_FORWARD_BACK &&
       tab_helper->has_failed_upgrade(handle->GetURL())) &&
      !handle->GetURL().SchemeIsCryptographic()) {
    if (IsInterstitialEnabled(interstitial_state_) &&
        !(ShouldExcludeUrlFromInterstitial(interstitial_state_,
                                           handle->GetURL()))) {
      security_interstitials::https_only_mode::RecordInterstitialReason(
          interstitial_state_);

      // Mark this as a fallback HTTP navigation and trigger the interstitial.
      tab_helper->set_is_navigation_fallback(true);
      std::unique_ptr<security_interstitials::HttpsOnlyModeBlockingPage>
          blocking_page =
              blocking_page_factory_->CreateHttpsOnlyModeBlockingPage(
                  contents, handle->GetURL(), interstitial_state_);
      std::string interstitial_html = blocking_page->GetHTMLContents();
      security_interstitials::SecurityInterstitialTabHelper::
          AssociateBlockingPage(handle, std::move(blocking_page));
      return content::NavigationThrottle::ThrottleCheckResult(
          content::NavigationThrottle::CANCEL, net::ERR_BLOCKED_BY_CLIENT,
          interstitial_html);
    }

    // Otherwise, just record metrics and continue.
    // TODO(crbug.com/40904694): Record a separate histogram for Site Engagement
    // heuristic.
  }

  // TODO(crbug.com/40064769): There are some cases where the navigation may
  // "restart", such as if we encounter an exempted transient network error on
  // the upgraded HTTPS URL, show a net error page, and then reload the tab. In
  // these cases the navigation will proceed with the upgrade/fallback logic,
  // but the navigation timeout will no longer be set. Currently, re-starting
  // the timer here would trigger a DCHECK in NavigationRequest.

  // Navigation is HTTPS or an initial HTTP navigation (which will get
  // upgraded by the interceptor). Fallback HTTP navigations are handled in
  // WillRedirectRequest().
  return content::NavigationThrottle::ThrottleAction::PROCEED;
}

content::NavigationThrottle::ThrottleCheckResult
HttpsUpgradesNavigationThrottle::WillFailRequest() {
  // Fallback to HTTP on navigation failure is handled by
  // HttpsUpgradesInterceptor::MaybeCreateLoaderForResponse().
  return content::NavigationThrottle::PROCEED;
}

content::NavigationThrottle::ThrottleCheckResult
HttpsUpgradesNavigationThrottle::WillRedirectRequest() {
  // If the navigation is doing a fallback redirect to HTTP, trigger the HTTP
  // interstitial (if enabled). The interceptor creates a redirect for the
  // fallback navigation, which will trigger MaybeCreateLoader() in the
  // interceptor for the redirect but *doesn't* trigger WillStartRequest()
  // because it's all part of the same request.
  auto* handle = navigation_handle();
  auto* contents = handle->GetWebContents();
  auto* tab_helper = HttpsOnlyModeTabHelper::FromWebContents(contents);

  // Slow loading fallback URLs should be handled by the net stack.
  if (tab_helper->is_navigation_fallback()) {
    handle->CancelNavigationTimeout();
  }

  if (tab_helper->is_navigation_fallback() &&
      !handle->GetURL().SchemeIsCryptographic() &&
      IsInterstitialEnabled(interstitial_state_) &&
      !ShouldExcludeUrlFromInterstitial(interstitial_state_,
                                        handle->GetURL())) {
    security_interstitials::https_only_mode::RecordInterstitialReason(
        interstitial_state_);

    std::unique_ptr<security_interstitials::HttpsOnlyModeBlockingPage>
        blocking_page = blocking_page_factory_->CreateHttpsOnlyModeBlockingPage(
            contents, handle->GetURL(), interstitial_state_);
    std::string interstitial_html = blocking_page->GetHTMLContents();
    security_interstitials::SecurityInterstitialTabHelper::
        AssociateBlockingPage(handle, std::move(blocking_page));
    return content::NavigationThrottle::ThrottleCheckResult(
        content::NavigationThrottle::CANCEL, net::ERR_BLOCKED_BY_CLIENT,
        interstitial_html);
  }

  // Otherwise, just record metrics and continue.
  // TODO(crbug.com/40904694): Record a separate histogram for Site Engagement
  // heuristic.

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
  // The Interceptor logs the URLs that it sees and triggers fallback if it
  // encounters a redirect loop. Any cases that might not be caught by the
  // Interceptor should result in net::ERR_TOO_MANY_REDIRECTS, but in general
  // redirect loops should hit the cache and not cost too much. If they go too
  // long, the fallback timer will kick in. ERR_TOO_MANY_REDIRECTS should result
  // in the request failing and triggering fallback.
  if (tab_helper->is_navigation_upgraded()) {
    // Check if the timer is already started, as there may be additional
    // redirects on the navigation after the artificial upgrade redirect.
    bool timer_started =
        navigation_handle()->SetNavigationTimeout(g_fallback_delay);
    if (timer_started) {
      RecordHttpsFirstModeNavigation(Event::kUpgradeAttempted,
                                     interstitial_state_);
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
    RecordHttpsFirstModeNavigation(Event::kUpgradeSucceeded,
                                   interstitial_state_);
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
    base::TimeDelta timeout) {
  g_fallback_delay = timeout;
}
