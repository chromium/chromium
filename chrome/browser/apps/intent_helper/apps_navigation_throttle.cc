// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/intent_helper/apps_navigation_throttle.h"

#include <utility>

#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/browser_app_launcher.h"
#include "chrome/browser/apps/intent_helper/intent_picker_internal.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/web_app_launch_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "chrome/common/chrome_features.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/features.h"
#include "ui/gfx/image/image.h"
#include "url/origin.h"

namespace {

using ThrottleCheckResult = content::NavigationThrottle::ThrottleCheckResult;

}  // namespace

namespace apps {

// static
std::unique_ptr<content::NavigationThrottle>
AppsNavigationThrottle::MaybeCreate(content::NavigationHandle* handle) {
  // Don't handle navigations in subframes or main frames that are in a nested
  // frame tree (e.g. portals, fenced-frame). We specifically allow
  // prerendering navigations so that we can destroy the prerender. Opening an
  // app must only happen when the user intentionally navigates; however, for a
  // prerender, the prerender-activating navigation doesn't run throttles so we
  // must cancel it during initial loading to get a standard (non-prerendering)
  // navigation at link-click-time.
  if (!handle->IsInPrimaryMainFrame() && !handle->IsInPrerenderedMainFrame())
    return nullptr;

  content::WebContents* web_contents = handle->GetWebContents();
  if (!ShouldCheckAppsForUrl(web_contents))
    return nullptr;

  return std::make_unique<AppsNavigationThrottle>(handle);
}

AppsNavigationThrottle::AppsNavigationThrottle(
    content::NavigationHandle* navigation_handle)
    : content::NavigationThrottle(navigation_handle) {}

AppsNavigationThrottle::~AppsNavigationThrottle() = default;

const char* AppsNavigationThrottle::GetNameForLogging() {
  return "AppsNavigationThrottle";
}

ThrottleCheckResult AppsNavigationThrottle::WillStartRequest() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  starting_url_ = GetStartingGURL(navigation_handle());
  return HandleRequest();
}

ThrottleCheckResult AppsNavigationThrottle::WillRedirectRequest() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return HandleRequest();
}

bool AppsNavigationThrottle::ShouldCancelNavigation(
    content::NavigationHandle* handle) {
  return false;
}

bool AppsNavigationThrottle::ShouldShowDisablePage(
    content::NavigationHandle* handle) {
  return false;
}

ThrottleCheckResult AppsNavigationThrottle::MaybeShowCustomResult() {
  return content::NavigationThrottle::CANCEL_AND_IGNORE;
}

bool AppsNavigationThrottle::navigate_from_link() const {
  return navigate_from_link_;
}

ThrottleCheckResult AppsNavigationThrottle::HandleRequest() {
  content::NavigationHandle* handle = navigation_handle();
  // If the navigation won't update the current document, don't check intent for
  // the navigation.
  if (handle->IsSameDocument())
    return content::NavigationThrottle::PROCEED;

  content::WebContents* web_contents = handle->GetWebContents();
  const GURL& url = handle->GetURL();
  navigate_from_link_ = IsNavigateFromLink(handle);

  // Do not automatically launch the app if we shouldn't override url loading,
  // or if we don't have a browser, or we are already in an app browser.
  if (ShouldOverrideUrlLoading(starting_url_, url) &&
      !InAppBrowser(web_contents)) {
    // Handles apps that are automatically launched and the navigation needs to
    // be cancelled. This only applies on the new intent picker system, because
    // we don't need to defer the navigation to find out preferred app anymore.
    if (ShouldCancelNavigation(handle)) {
      return content::NavigationThrottle::CANCEL_AND_IGNORE;
    }

    // Handles web app link capturing that has not yet integrated with the
    // intent handling system.
    // TODO(crbug.com/1163398): Remove this code path.
    absl::optional<ThrottleCheckResult> web_app_capture =
        CaptureWebAppScopeNavigations(web_contents, handle);
    if (web_app_capture.has_value())
      return web_app_capture.value();

    if (ShouldShowDisablePage(handle))
      return MaybeShowCustomResult();
  }

  return content::NavigationThrottle::PROCEED;
}

absl::optional<ThrottleCheckResult>
AppsNavigationThrottle::CaptureWebAppScopeNavigations(
    content::WebContents* web_contents,
    content::NavigationHandle* handle) const {
  if (!navigate_from_link())
    return absl::nullopt;

  Profile* const profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  web_app::WebAppProvider* provider =
      web_app::WebAppProvider::GetForWebApps(profile);
  if (!provider)
    return absl::nullopt;

  absl::optional<web_app::AppId> app_id =
      provider->registrar().FindInstalledAppWithUrlInScope(
          handle->GetURL(), /*window_only=*/true);
  if (!app_id)
    return absl::nullopt;

  // Experimental tabbed web app link capturing behaves like new-client.
  // This will be removed once we phase out kDesktopPWAsTabStripLinkCapturing in
  // favor of kWebAppEnableLinkCapturing.
  bool app_in_tabbed_mode =
      provider->registrar().IsTabbedWindowModeEnabled(*app_id);
  bool tabbed_link_capturing =
      base::FeatureList::IsEnabled(features::kDesktopPWAsTabStripLinkCapturing);

  auto* tab_helper = web_app::WebAppTabHelper::FromWebContents(web_contents);
  if (tab_helper && tab_helper->GetAppId() == *app_id) {
    // Already in app scope, do not alter window state while using the app.
    return absl::nullopt;
  }

  blink::mojom::CaptureLinks capture_links =
      provider->registrar().GetAppById(*app_id)->capture_links();

  if (capture_links == blink::mojom::CaptureLinks::kUndefined &&
      app_in_tabbed_mode && tabbed_link_capturing) {
    capture_links = blink::mojom::CaptureLinks::kNewClient;
  }

  switch (capture_links) {
    case blink::mojom::CaptureLinks::kUndefined:
    case blink::mojom::CaptureLinks::kNone:
      return absl::nullopt;

    case blink::mojom::CaptureLinks::kExistingClientNavigate:
    case blink::mojom::CaptureLinks::kNewClient: {
      Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
      if (!browser) {
        // This is a middle click open in new tab action; do not capture.
        return absl::nullopt;
      }

      if (web_app::AppBrowserController::IsForWebApp(browser, *app_id)) {
        // Already in the app window; navigation already captured.
        return absl::nullopt;
      }

      if (handle->IsInPrerenderedMainFrame()) {
        // If this is a prerender navigation that would otherwise launch an
        // app, we must cancel it. We only want to launch an app once the URL
        // is intentionally navigated to by the user. We cancel the navigation
        // here so that when the link is clicked, we'll run NavigationThrottles
        // again. If we leave the prerendering alive, the activating navigation
        // won't run throttles.
        return content::NavigationThrottle::CANCEL_AND_IGNORE;
      }

      if (capture_links ==
          blink::mojom::CaptureLinks::kExistingClientNavigate) {
        for (Browser* open_browser : *BrowserList::GetInstance()) {
          if (web_app::AppBrowserController::IsForWebApp(open_browser,
                                                         *app_id)) {
            open_browser->OpenURL(
                content::OpenURLParams::FromNavigationHandle(handle));

            // If |web_contents| hasn't loaded yet or has only loaded
            // about:blank we should remove it to avoid leaving behind a blank
            // tab.
            if (tab_helper && !tab_helper->HasLoadedNonAboutBlankPage())
              web_contents->ClosePage();

            return content::NavigationThrottle::CANCEL_AND_IGNORE;
          }
        }
        // No browser found; fallthrough to new-client behaviour.
      }

      // If |web_contents| hasn't loaded yet or has only loaded about:blank we
      // should reparent it into the app window to avoid leaving behind a blank
      // tab.
      if (tab_helper && !tab_helper->HasLoadedNonAboutBlankPage()) {
        web_app::ReparentWebContentsIntoAppBrowser(web_contents, *app_id);
        return content::NavigationThrottle::PROCEED;
      }

      apps::AppLaunchParams launch_params(
          *app_id, apps::mojom::LaunchContainer::kLaunchContainerWindow,
          WindowOpenDisposition::NEW_FOREGROUND_TAB,
          apps::mojom::LaunchSource::kFromUrlHandler);
      launch_params.override_url = handle->GetURL();
      apps::AppServiceProxyFactory::GetForProfile(profile)
          ->BrowserAppLauncher()
          ->LaunchAppWithParams(std::move(launch_params));
      return content::NavigationThrottle::CANCEL_AND_IGNORE;
    }
  }
}

}  // namespace apps
