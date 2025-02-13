// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/link_capturing/chromeos_reimpl_navigation_capturing_throttle.h"

#include <memory>
#include <string>

#include "ash/constants/web_app_id_constants.h"
#include "ash/webui/projector_app/public/cpp/projector_app_constants.h"
#include "base/auto_reset.h"
#include "base/check_is_test.h"
#include "base/memory/ptr_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/default_tick_clock.h"
#include "base/time/tick_clock.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/apps/link_capturing/metrics/intent_handling_metrics.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"  // nogncheck https://crbug.com/1474116
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"  // nogncheck https://crbug.com/1474116
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/link_capturing_features.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/page_transition_types.h"

namespace apps {

namespace {

using ThrottleCheckResult = content::NavigationThrottle::ThrottleCheckResult;

bool IsSystemWebApp(Profile* profile, const std::string& app_id) {
  bool is_system_web_app = false;
  apps::AppServiceProxyFactory::GetForProfile(profile)
      ->AppRegistryCache()
      .ForOneApp(app_id, [&is_system_web_app](const apps::AppUpdate& update) {
        if (update.InstallReason() == apps::InstallReason::kSystem) {
          is_system_web_app = true;
        }
      });
  return is_system_web_app;
}

// Used to create a unique timestamped URL to force reload apps.
// Points to the base::DefaultTickClock by default.
static const base::TickClock*& GetTickClock() {
  static const base::TickClock* g_clock = base::DefaultTickClock::GetInstance();
  return g_clock;
}

// This function redirects an external untrusted |url| to a privileged trusted
// one for SWAs, if applicable.
GURL RedirectUrlIfProjectorApp(Profile* profile,
                               const std::string& app_id,
                               const GURL& url) {
  if (!IsSystemWebApp(profile, app_id)) {
    return url;
  }

  bool is_projector_app =
      app_id == ash::kChromeUIUntrustedProjectorSwaAppId &&
      url.GetWithEmptyPath() == GURL(ash::kChromeUIUntrustedProjectorPwaUrl);
  if (!is_projector_app) {
    return url;
  }

  // Handle projector app redirection.
  std::string override_url = ash::kChromeUIUntrustedProjectorUrl;
  if (url.path().length() > 1) {
    override_url += url.path().substr(1);
  }
  std::stringstream ss;
  // Since ChromeOS doesn't reload an app if the URL doesn't change, the line
  // below appends a unique timestamp to the URL to force a reload.
  // TODO(b/211787536): Remove the timestamp after we update the trusted URL
  // to match the user's navigations through the post message api.
  ss << override_url << "?timestamp=" << GetTickClock()->NowTicks();

  if (url.has_query()) {
    ss << '&' << url.query();
  }

  GURL result(ss.str());
  CHECK(result.is_valid());
  return result;
}

IntentHandlingMetrics::Platform GetMetricsPlatform(AppType app_type) {
  switch (app_type) {
    case AppType::kArc:
      return IntentHandlingMetrics::Platform::ARC;
    case AppType::kWeb:
    case AppType::kSystemWeb:
      return IntentHandlingMetrics::Platform::PWA;
    case AppType::kUnknown:
    case AppType::kCrostini:
    case AppType::kChromeApp:
    case AppType::kPluginVm:
    case AppType::kRemote:
    case AppType::kBorealis:
    case AppType::kExtension:
    case AppType::kBruschetta:
      NOTREACHED();
  }
}

bool IsNavigationUserInitiated(content::NavigationHandle* handle) {
  switch (handle->GetNavigationInitiatorActivationAndAdStatus()) {
    case blink::mojom::NavigationInitiatorActivationAndAdStatus::
        kDidNotStartWithTransientActivation:
      return false;
    case blink::mojom::NavigationInitiatorActivationAndAdStatus::
        kStartedWithTransientActivationFromNonAd:
    case blink::mojom::NavigationInitiatorActivationAndAdStatus::
        kStartedWithTransientActivationFromAd:
      return true;
  }
}

void LaunchApp(base::WeakPtr<AppServiceProxy> proxy,
               const std::string& app_id,
               int32_t event_flags,
               GURL url,
               LaunchSource launch_source,
               WindowInfoPtr window_info,
               AppType app_type,
               std::unique_ptr<ScopedKeepAlive>,
               std::unique_ptr<ScopedProfileKeepAlive>) {
  if (!proxy) {
    return;
  }

  proxy->LaunchAppWithUrl(app_id, event_flags, url, launch_source,
                          std::move(window_info), base::DoNothing());

  IntentHandlingMetrics::RecordPreferredAppLinkClickMetrics(
      GetMetricsPlatform(app_type));
}

}  // namespace

// static
std::unique_ptr<content::NavigationThrottle>
ChromeOsReimplNavigationCapturingThrottle::MaybeCreate(
    content::NavigationHandle* handle) {
  if (!features::IsNavigationCapturingReimplEnabled()) {
    return nullptr;
  }

  content::WebContents* contents = handle->GetWebContents();
  Profile* profile = Profile::FromBrowserContext(contents->GetBrowserContext());
  if (!AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile)) {
    return nullptr;
  }

  return base::WrapUnique(
      new ChromeOsReimplNavigationCapturingThrottle(handle, profile));
}

ChromeOsReimplNavigationCapturingThrottle::
    ~ChromeOsReimplNavigationCapturingThrottle() = default;

const char* ChromeOsReimplNavigationCapturingThrottle::GetNameForLogging() {
  return "ChromeOsReimplNavigationCapturingThrottle";
}

// static
base::AutoReset<const base::TickClock*>
ChromeOsReimplNavigationCapturingThrottle::SetClockForTesting(
    const base::TickClock* tick_clock) {
  CHECK_IS_TEST();
  return base::AutoReset<const base::TickClock*>(&GetTickClock(), tick_clock);
}

ThrottleCheckResult
ChromeOsReimplNavigationCapturingThrottle::WillStartRequest() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(&profile_.get());
  if (!proxy) {
    return content::NavigationThrottle::PROCEED;
  }

  CHECK(navigation_handle());
  content::NavigationHandle* handle = navigation_handle();
  AppIdsToLaunchForUrl app_ids_to_launch =
      FindAppIdsToLaunchForUrl(proxy, handle->GetURL());

  const std::vector<std::string>& app_candidates = app_ids_to_launch.candidates;
  // If there are no candidates for launching the url in an app or the app is
  // not preferred for launching the url, allow navigation to proceed normally.
  if (app_candidates.empty() || !app_ids_to_launch.preferred) {
    return content::NavigationThrottle::PROCEED;
  }

  const std::string launch_app_id = *app_ids_to_launch.preferred;
  const AppType app_type = proxy->AppRegistryCache().GetAppType(launch_app_id);
  const bool does_projector_swa_handle_url =
      base::Contains(app_candidates, ash::kChromeUIUntrustedProjectorSwaAppId);
  const bool is_app_capturable =
      (app_type == AppType::kArc) || does_projector_swa_handle_url;

  if (!is_app_capturable) {
    return content::NavigationThrottle::PROCEED;
  }

  auto launch_source = IsCapturableLinkClick() ? LaunchSource::kFromLink
                                               : LaunchSource::kFromOmnibox;
  GURL redirected_url =
      does_projector_swa_handle_url
          ? RedirectUrlIfProjectorApp(&profile_.get(), launch_app_id,
                                      handle->GetURL())
          : handle->GetURL();

  // Close existing web contents if it is around.
  std::unique_ptr<ScopedKeepAlive> browser_keep_alive;
  std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive;
  if (IsEmptyDanglingWebContentsAfterLinkCapture()) {
    browser_keep_alive = std::make_unique<ScopedKeepAlive>(
        KeepAliveOrigin::APP_LAUNCH, KeepAliveRestartOption::ENABLED);
    if (!profile_->IsOffTheRecord()) {
      profile_keep_alive = std::make_unique<ScopedProfileKeepAlive>(
          &profile_.get(), ProfileKeepAliveOrigin::kAppWindow);
    }
    handle->GetWebContents()->ClosePage();
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&LaunchApp, proxy->GetWeakPtr(), launch_app_id,
                     GetEventFlags(WindowOpenDisposition::NEW_WINDOW,
                                   /*prefer_container=*/true),
                     redirected_url, launch_source,
                     std::make_unique<WindowInfo>(display::kDefaultDisplayId),
                     app_type, std::move(browser_keep_alive),
                     std::move(profile_keep_alive)));

  return content::NavigationThrottle::CANCEL_AND_IGNORE;
}

ThrottleCheckResult
ChromeOsReimplNavigationCapturingThrottle::WillRedirectRequest() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return content::NavigationThrottle::PROCEED;
}

ChromeOsReimplNavigationCapturingThrottle::
    ChromeOsReimplNavigationCapturingThrottle(
        content::NavigationHandle* navigation_handle,
        Profile* profile)
    : content::NavigationThrottle(navigation_handle), profile_(*profile) {}

bool ChromeOsReimplNavigationCapturingThrottle::
    IsEmptyDanglingWebContentsAfterLinkCapture() {
  CHECK(navigation_handle());
  content::WebContents* contents = navigation_handle()->GetWebContents();
  CHECK(contents);

  const GURL& last_committed_url = contents->GetLastCommittedURL();
  return !last_committed_url.is_valid() || last_committed_url.IsAboutBlank() ||
         // Some navigations are via JavaScript `location.href = url;`.
         // This can be used for user clicked buttons as well as redirects.
         // Check whether the action was in the context of a user activation to
         // distinguish redirects from click event handlers.
         !IsNavigationUserInitiated(navigation_handle());
}

bool ChromeOsReimplNavigationCapturingThrottle::IsCapturableLinkClick() {
  CHECK(navigation_handle());

  if (navigation_handle()->WasStartedFromContextMenu()) {
    return false;
  }

  ui::PageTransition page_transition = navigation_handle()->GetPageTransition();
  // Mask out redirect qualifiers from page transition.
  page_transition = ui::PageTransitionFromInt(
      page_transition & ~ui::PAGE_TRANSITION_IS_REDIRECT_MASK);
  if (!ui::PageTransitionCoreTypeIs(page_transition,
                                    ui::PAGE_TRANSITION_LINK)) {
    return false;
  }

  if (base::to_underlying(ui::PageTransitionGetQualifier(page_transition)) !=
      0) {
    // Qualifiers indicate that this navigation was the result of a click on a
    // forward/back button, or typing in the URL bar. Don't handle any of those
    // types of navigations.
    return false;
  }

  return true;
}

}  // namespace apps
