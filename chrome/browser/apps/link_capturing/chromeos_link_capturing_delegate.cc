// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/link_capturing/chromeos_link_capturing_delegate.h"

#include "ash/webui/projector_app/public/cpp/projector_app_constants.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/values_equivalent.h"
#include "base/time/default_tick_clock.h"
#include "base/time/tick_clock.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/apps/intent_helper/metrics/intent_handling_metrics.h"  // nogncheck https://crbug.com/1474116
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace apps {
namespace {
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

void LaunchApp(base::WeakPtr<AppServiceProxy> proxy,
               const std::string& app_id,
               int32_t event_flags,
               GURL url,
               LaunchSource launch_source,
               WindowInfoPtr window_info,
               AppType app_type,
               base::OnceClosure callback) {
  if (!proxy) {
    std::move(callback).Run();
    return;
  }

  proxy->LaunchAppWithUrl(
      app_id, event_flags, url, launch_source, std::move(window_info),
      base::IgnoreArgs<LaunchResult&&>(std::move(callback)));

  IntentHandlingMetrics::RecordPreferredAppLinkClickMetrics(
      GetMetricsPlatform(app_type));
}
}  // namespace

// static
const base::TickClock* ChromeOsLinkCapturingDelegate::clock_ =
    base::DefaultTickClock::GetInstance();

// static
void ChromeOsLinkCapturingDelegate::SetClockForTesting(
    const base::TickClock* tick_clock) {
  clock_ = tick_clock;
}

ChromeOsLinkCapturingDelegate::ChromeOsLinkCapturingDelegate() = default;
ChromeOsLinkCapturingDelegate::~ChromeOsLinkCapturingDelegate() = default;

bool ChromeOsLinkCapturingDelegate::ShouldCancelThrottleCreation(
    content::NavigationHandle* handle) {
  content::WebContents* web_contents = handle->GetWebContents();
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  return !AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile);
}

absl::optional<apps::LinkCapturingNavigationThrottle::LaunchCallback>
ChromeOsLinkCapturingDelegate::CreateLinkCaptureLaunchClosure(
    Profile* profile,
    content::WebContents* web_contents,
    const GURL& url,
    bool is_navigation_from_link) {
  AppServiceProxy* proxy = apps::AppServiceProxyFactory::GetForProfile(profile);

  AppIdsToLaunchForUrl app_id_to_launch = FindAppIdsToLaunchForUrl(proxy, url);

  if (app_id_to_launch.candidates.empty()) {
    return absl::nullopt;
  }

  if (ShouldOnlyCaptureLinks(app_id_to_launch.candidates) &&
      !is_navigation_from_link) {
    return absl::nullopt;
  }

  if (!app_id_to_launch.preferred) {
    return absl::nullopt;
  }

  const std::string& preferred_app_id = *app_id_to_launch.preferred;
  // Only automatically launch supported app types.
  AppType app_type = proxy->AppRegistryCache().GetAppType(preferred_app_id);
  if (app_type != AppType::kArc && app_type != AppType::kWeb &&
      !IsSystemWebApp(profile, preferred_app_id)) {
    return absl::nullopt;
  }

  // Don't capture if already inside the target app scope.
  if (app_type == AppType::kWeb &&
      base::ValuesEquivalent(web_app::WebAppTabHelper::GetAppId(web_contents),
                             &preferred_app_id)) {
    return absl::nullopt;
  }

  auto launch_source = is_navigation_from_link ? LaunchSource::kFromLink
                                               : LaunchSource::kFromOmnibox;
  GURL redirected_url =
      RedirectUrlIfSwa(profile, preferred_app_id, url, clock_);

  // Note: The launch can occur after this object is destroyed, so bind to a
  // static function.
  return base::BindOnce(
      &LaunchApp, proxy->GetWeakPtr(), preferred_app_id,
      GetEventFlags(WindowOpenDisposition::NEW_WINDOW,
                    /*prefer_container=*/true),
      redirected_url, launch_source,
      std::make_unique<WindowInfo>(display::kDefaultDisplayId), app_type);
}

}  // namespace apps
