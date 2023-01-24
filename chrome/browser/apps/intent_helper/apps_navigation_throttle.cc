// Copyright 2018 The Chromium Authors
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

    if (ShouldShowDisablePage(handle))
      return MaybeShowCustomResult();
  }

  return content::NavigationThrottle::PROCEED;
}

}  // namespace apps
