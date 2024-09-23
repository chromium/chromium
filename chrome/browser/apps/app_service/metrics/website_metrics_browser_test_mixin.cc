// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/metrics/website_metrics_browser_test_mixin.h"

#include <utility>

#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/metrics/website_metrics.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_navigation_observer.h"
#include "ui/base/window_open_disposition.h"
#include "ui/wm/public/activation_client.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/apps/app_service/app_service_proxy_ash.h"
#include "chrome/browser/apps/app_service/metrics/app_platform_metrics_service.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/apps/app_service/app_service_proxy_lacros.h"
#include "chrome/browser/apps/app_service/metrics/website_metrics_service_lacros.h"
#endif

namespace apps {

WebsiteMetricsBrowserTestMixin::WebsiteMetricsBrowserTestMixin(
    InProcessBrowserTestMixinHost* host)
    : InProcessBrowserTestMixin(host) {}

WebsiteMetricsBrowserTestMixin::~WebsiteMetricsBrowserTestMixin() = default;

void WebsiteMetricsBrowserTestMixin::SetUpOnMainThread() {
  auto* const profile = ProfileManager::GetPrimaryUserProfile();
  if (!::apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(
          profile)) {
    // Cannot initialize website metrics for the profile. Return.
    return;
  }

  auto* const app_service_proxy =
      ::apps::AppServiceProxyFactory::GetForProfile(profile);
  CHECK(app_service_proxy);
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Instantiate website metrics service and website metrics component only if
  // one does not exist already. This ensures observers continue observing
  // pre-existing sources.
  website_metrics_service_ = app_service_proxy->WebsiteMetricsService();
  if (!website_metrics_service_) {
    auto metrics_service =
        std::make_unique<WebsiteMetricsServiceLacros>(profile);
    website_metrics_service_ = metrics_service.get();
    app_service_proxy->SetWebsiteMetricsServiceForTesting(
        std::move(metrics_service));
  }
  if (!website_metrics_service_->WebsiteMetrics()) {
    auto website_metrics =
        std::make_unique<WebsiteMetrics>(profile,
                                         /*user_type_by_device_type=*/0);
    website_metrics_service_->SetWebsiteMetricsForTesting(
        std::move(website_metrics));
  }
  website_metrics_service_->Start();
#else
  // Instantiate app platform metrics service only if one does not exist
  // already. This ensures observers continue observing the same source while
  // retrieving the initialized website metrics component.
  app_platform_metrics_service_ =
      app_service_proxy->AppPlatformMetricsService();
  if (!app_platform_metrics_service_) {
    auto metrics_service = std::make_unique<AppPlatformMetricsService>(profile);
    app_platform_metrics_service_ = metrics_service.get();
    app_service_proxy->SetAppPlatformMetricsServiceForTesting(
        std::move(metrics_service));
  }
  app_platform_metrics_service_->Start(
      app_service_proxy->AppRegistryCache(),
      app_service_proxy->InstanceRegistry(),
      app_service_proxy->AppCapabilityAccessCache());
#endif
}

Browser* WebsiteMetricsBrowserTestMixin::CreateBrowser() {
  DCHECK_CURRENTLY_ON(::content::BrowserThread::UI);
  auto* const profile = ProfileManager::GetPrimaryUserProfile();
  CHECK(profile);
  Browser::CreateParams params(profile, /*user_gesture=*/true);

  // Create a new browser instance. The subsequent `BrowserWindow` that was
  // created as part of this instantiation will own the browser instance.
  Browser* const browser = Browser::Create(params);
  browser->window()->Show();
  auto* const window = browser->window()->GetNativeWindow();
  wm::GetActivationClient(window->GetRootWindow())->ActivateWindow(window);
  return browser;
}

::content::WebContents* WebsiteMetricsBrowserTestMixin::NavigateAndWait(
    Browser* browser,
    const std::string& url,
    WindowOpenDisposition disposition) {
  NavigateParams params(browser, GURL(url),
                        ::ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
  params.disposition = disposition;
  Navigate(&params);
  auto* const contents = params.navigated_or_inserted_contents.get();
  CHECK_EQ(::chrome::FindBrowserWithTab(params.navigated_or_inserted_contents),
           browser);
  ::content::TestNavigationObserver observer(contents);
  observer.Wait();
  return contents;
}

void WebsiteMetricsBrowserTestMixin::NavigateActiveTab(Browser* browser,
                                                       const std::string& url) {
  NavigateAndWait(browser, url, WindowOpenDisposition::CURRENT_TAB);
}

::content::WebContents* WebsiteMetricsBrowserTestMixin::InsertForegroundTab(
    Browser* browser,
    const std::string& url) {
  return NavigateAndWait(browser, url,
                         WindowOpenDisposition::NEW_FOREGROUND_TAB);
}

::content::WebContents* WebsiteMetricsBrowserTestMixin::InsertBackgroundTab(
    Browser* browser,
    const std::string& url) {
  return NavigateAndWait(browser, url,
                         WindowOpenDisposition::NEW_BACKGROUND_TAB);
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
WebsiteMetricsServiceLacros* WebsiteMetricsBrowserTestMixin::metrics_service() {
  return website_metrics_service_;
}
#else
AppPlatformMetricsService* WebsiteMetricsBrowserTestMixin::metrics_service() {
  return app_platform_metrics_service_;
}
#endif

WebsiteMetrics* WebsiteMetricsBrowserTestMixin::website_metrics() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  CHECK(website_metrics_service_);
  return website_metrics_service_->WebsiteMetrics();
#else
  CHECK(app_platform_metrics_service_);
  return app_platform_metrics_service_->WebsiteMetrics();
#endif
}

}  // namespace apps
