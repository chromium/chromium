// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/intent_helper/chromeos_apps_navigation_throttle.h"

#include <memory>
#include <sstream>
#include <string>
#include <utility>

#include "ash/webui/projector_app/public/cpp/projector_app_constants.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/values_equivalent.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner_helpers.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/default_tick_clock.h"
#include "base/time/tick_clock.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/apps/intent_helper/apps_navigation_types.h"
#include "chrome/browser/apps/intent_helper/chromeos_intent_picker_helpers.h"
#include "chrome/browser/apps/intent_helper/intent_picker_internal.h"
#include "chrome/browser/apps/intent_helper/metrics/intent_handling_metrics.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/system_features_disable_list_policy_handler.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/web_applications/web_app_launch_utils.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_id_constants.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/browser_resources.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/jstemplate_builder.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/display/types/display_constants.h"

namespace apps {

namespace {

using ThrottleCheckResult = content::NavigationThrottle::ThrottleCheckResult;

// TODO(crbug.com/1251490 ) Update to make disabled page works in Lacros.
std::string GetAppDisabledErrorPage() {
  base::Value::Dict strings;

  strings.Set("disabledPageHeader",
              l10n_util::GetStringUTF16(IDS_CHROME_URLS_DISABLED_PAGE_HEADER));
  strings.Set("disabledPageTitle",
              l10n_util::GetStringUTF16(IDS_CHROME_URLS_DISABLED_PAGE_TITLE));
  strings.Set("disabledPageMessage",
              l10n_util::GetStringUTF16(IDS_CHROME_URLS_DISABLED_PAGE_MESSAGE));
  std::string html =
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
          IDR_CHROME_URLS_DISABLED_PAGE_HTML);
  const std::string& app_locale = g_browser_process->GetApplicationLocale();
  webui::SetLoadTimeDataDefaults(app_locale, &strings);
  return webui::GetI18nTemplateHtml(html, strings);
}

bool IsAppDisabled(const std::string& app_id) {
  policy::SystemFeature system_feature =
      policy::SystemFeaturesDisableListPolicyHandler::GetSystemFeatureFromAppId(
          app_id);

  if (system_feature == policy::SystemFeature::kUnknownSystemFeature) {
    return false;
  }

  return policy::SystemFeaturesDisableListPolicyHandler::
      IsSystemFeatureDisabled(system_feature, g_browser_process->local_state());
}

// Usually we want to only capture navigations from clicking a link. For a
// subset of apps, we want to capture typing into the omnibox as well.
bool ShouldOnlyCaptureLinks(const std::vector<std::string>& app_ids) {
  for (const auto& app_id : app_ids) {
    if (app_id == ash::kChromeUIUntrustedProjectorSwaAppId) {
      return false;
    }
  }
  return true;
}

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

// This function redirects an external untrusted |url| to a privileged trusted
// one for SWAs, if applicable.
GURL RedirectUrlIfSwa(Profile* profile,
                      const std::string& app_id,
                      const GURL& url,
                      const base::TickClock* clock) {
  if (!IsSystemWebApp(profile, app_id)) {
    return url;
  }

  // Projector:
  if (app_id == ash::kChromeUIUntrustedProjectorSwaAppId &&
      url.GetWithEmptyPath() == GURL(ash::kChromeUIUntrustedProjectorPwaUrl)) {
    std::string override_url = ash::kChromeUIUntrustedProjectorUrl;
    if (url.path().length() > 1) {
      override_url += url.path().substr(1);
    }
    std::stringstream ss;
    // Since ChromeOS doesn't reload an app if the URL doesn't change, the line
    // below appends a unique timestamp to the URL to force a reload.
    // TODO(b/211787536): Remove the timestamp after we update the trusted URL
    // to match the user's navigations through the post message api.
    ss << override_url << "?timestamp=" << clock->NowTicks();

    if (url.has_query()) {
      ss << '&' << url.query();
    }

    GURL result(ss.str());
    DCHECK(result.is_valid());
    return result;
  }
  // Add redirects for other SWAs above this line.

  // No matching SWAs found, returning original url.
  return url;
}

IntentHandlingMetrics::Platform GetMetricsPlatform(AppType app_type) {
  switch (app_type) {
    case AppType::kArc:
      return IntentHandlingMetrics::Platform::ARC;
    case AppType::kWeb:
    case AppType::kSystemWeb:
      return IntentHandlingMetrics::Platform::PWA;
    case AppType::kUnknown:
    case AppType::kBuiltIn:
    case AppType::kCrostini:
    case AppType::kChromeApp:
    case AppType::kMacOs:
    case AppType::kPluginVm:
    case AppType::kStandaloneBrowser:
    case AppType::kRemote:
    case AppType::kBorealis:
    case AppType::kStandaloneBrowserChromeApp:
    case AppType::kExtension:
    case AppType::kStandaloneBrowserExtension:
    case AppType::kBruschetta:
      NOTREACHED();
      return IntentHandlingMetrics::Platform::ARC;
  }
}

}  // namespace

// static
std::unique_ptr<apps::AppsNavigationThrottle>
ChromeOsAppsNavigationThrottle::MaybeCreate(content::NavigationHandle* handle) {
  // Don't handle navigations in subframes or main frames that are in a nested
  // frame tree (e.g. portals, fenced-frame). We specifically allow
  // prerendering navigations so that we can destroy the prerender. Opening an
  // app must only happen when the user intentionally navigates; however, for a
  // prerender, the prerender-activating navigation doesn't run throttles so we
  // must cancel it during initial loading to get a standard (non-prerendering)
  // navigation at link-click-time.
  if (!handle->IsInPrimaryMainFrame() && !handle->IsInPrerenderedMainFrame()) {
    return nullptr;
  }

  content::WebContents* web_contents = handle->GetWebContents();

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());

  if (!AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile)) {
    return nullptr;
  }

  if (!ShouldCheckAppsForUrl(web_contents)) {
    return nullptr;
  }

  return std::make_unique<ChromeOsAppsNavigationThrottle>(handle);
}

// static
const base::TickClock* ChromeOsAppsNavigationThrottle::clock_ =
    base::DefaultTickClock::GetInstance();

// static
void ChromeOsAppsNavigationThrottle::SetClockForTesting(
    const base::TickClock* tick_clock) {
  clock_ = tick_clock;
}

base::OnceClosure&
ChromeOsAppsNavigationThrottle::GetLinkCaptureLaunchCallbackForTesting() {
  static base::NoDestructor<base::OnceClosure> callback;
  return *callback;
}

ChromeOsAppsNavigationThrottle::ChromeOsAppsNavigationThrottle(
    content::NavigationHandle* navigation_handle)
    : apps::AppsNavigationThrottle(navigation_handle) {}

ChromeOsAppsNavigationThrottle::~ChromeOsAppsNavigationThrottle() = default;

bool ChromeOsAppsNavigationThrottle::ShouldCancelNavigation(
    content::NavigationHandle* handle) {
  content::WebContents* web_contents = handle->GetWebContents();

  const GURL& url = handle->GetURL();

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());

  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile);

  AppIdsToLaunchForUrl app_id_to_launch = FindAppIdsToLaunchForUrl(proxy, url);

  if (app_id_to_launch.candidates.empty()) {
    return false;
  }

  if (ShouldOnlyCaptureLinks(app_id_to_launch.candidates) &&
      !navigate_from_link()) {
    return false;
  }

  if (!app_id_to_launch.preferred) {
    return false;
  }

  const std::string& preferred_app_id = *app_id_to_launch.preferred;
  // Only automatically launch supported app types.
  auto app_type = proxy->AppRegistryCache().GetAppType(preferred_app_id);
  if (app_type != AppType::kArc && app_type != AppType::kWeb &&
      !IsSystemWebApp(profile, preferred_app_id)) {
    return false;
  }

  // Don't capture if already inside the target app scope.
  if (app_type == AppType::kWeb &&
      base::ValuesEquivalent(web_app::WebAppTabHelper::GetAppId(web_contents),
                             &preferred_app_id)) {
    return false;
  }

  // If this is a prerender navigation that would otherwise launch an app, we
  // must cancel it. We only want to launch an app once the URL is
  // intentionally navigated to by the user. We cancel the navigation here so
  // that when the link is clicked, we'll run NavigationThrottles again. If we
  // leave the prerendering alive, the activating navigation won't run
  // throttles.
  if (handle->IsInPrerenderedMainFrame()) {
    return true;
  }

  // Browser & profile keep-alives must be used to keep the browser & profile
  // alive because the old window is required to be closed before the new app is
  // launched, which will destroy the profile & browser if it is the last
  // window.
  // Why close the tab first? The way web contents currently work, closing a tab
  // in a window will re-activate that window if there are more tabs there. So
  // if we wait until after the launch completes to close the tab, then it will
  // cause the old window to come to the front hiding the newly launched app
  // window.
  std::unique_ptr<ScopedKeepAlive> browser_keep_alive;
  std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive;
  const GURL& last_committed_url = web_contents->GetLastCommittedURL();
  if (!last_committed_url.is_valid() || last_committed_url.IsAboutBlank() ||
      // After clicking a link in various apps (eg gchat), a blank redirect page
      // is left behind. Remove it to clean up. WasInitiatedByLinkClick()
      // returns false for links clicked from apps.
      !handle->WasInitiatedByLinkClick()) {
    browser_keep_alive = std::make_unique<ScopedKeepAlive>(
        KeepAliveOrigin::APP_LAUNCH, KeepAliveRestartOption::ENABLED);
    if (!profile->IsOffTheRecord()) {
      profile_keep_alive = std::make_unique<ScopedProfileKeepAlive>(
          profile, ProfileKeepAliveOrigin::kAppWindow);
    }
    web_contents->ClosePage();
  }

  auto launch_source = navigate_from_link() ? LaunchSource::kFromLink
                                            : LaunchSource::kFromOmnibox;
  GURL redirected_url =
      RedirectUrlIfSwa(profile, preferred_app_id, url, clock_);
  // The tab may have been closed, which runs async and causes the browser
  // window to be refocused. Post a task to launch the app to ensure launching
  // happens after the tab closed, otherwise the opened app window might be
  // inactivated.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &AppServiceProxy::LaunchAppWithUrl, proxy->GetWeakPtr(),
          preferred_app_id,
          GetEventFlags(WindowOpenDisposition::NEW_WINDOW,
                        /*prefer_container=*/true),
          redirected_url, launch_source,
          std::make_unique<WindowInfo>(display::kDefaultDisplayId),
          base::BindOnce(
              [](std::unique_ptr<ScopedKeepAlive> browser_keep_alive,
                 std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive,
                 LaunchResult&&) {
                // Note: This function currently only serves to own the "keep
                // alive" objects until the launch is complete.
                if (GetLinkCaptureLaunchCallbackForTesting()) {
                  std::move(GetLinkCaptureLaunchCallbackForTesting()).Run();
                }
              },
              std::move(browser_keep_alive), std::move(profile_keep_alive))));

  IntentHandlingMetrics::RecordPreferredAppLinkClickMetrics(
      GetMetricsPlatform(app_type));
  return true;
}

bool ChromeOsAppsNavigationThrottle::ShouldShowDisablePage(
    content::NavigationHandle* handle) {
  content::WebContents* web_contents = handle->GetWebContents();
  const GURL& url = handle->GetURL();

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  std::vector<std::string> app_ids =
      apps::AppServiceProxyFactory::GetForProfile(profile)->GetAppIdsForUrl(
          url, /*exclude_browsers=*/true, /*exclude_browser_tab_apps=*/false);

  for (const auto& app_id : app_ids) {
    if (IsAppDisabled(app_id)) {
      return true;
    }
  }
  return false;
}

ThrottleCheckResult ChromeOsAppsNavigationThrottle::MaybeShowCustomResult() {
  return ThrottleCheckResult(content::NavigationThrottle::CANCEL,
                             net::ERR_BLOCKED_BY_ADMINISTRATOR,
                             GetAppDisabledErrorPage());
}

}  // namespace apps
