// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_METRICS_WEBSITE_METRICS_BROWSER_TEST_MIXIN_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_METRICS_WEBSITE_METRICS_BROWSER_TEST_MIXIN_H_

#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/metrics/website_metrics.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/window_open_disposition.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/apps/app_service/metrics/app_platform_metrics_service.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/apps/app_service/metrics/website_metrics_service_lacros.h"
#endif

namespace apps {

// A browser test mixin that can be used to set up the `WebsiteMetrics`
// component for the primary user profile (if supported). The mixin also
// includes relevant accessors and helpers for interacting with the website
// metrics component (if initialized) as well as other browser components.
class WebsiteMetricsBrowserTestMixin : public InProcessBrowserTestMixin {
 public:
  explicit WebsiteMetricsBrowserTestMixin(InProcessBrowserTestMixinHost* host);
  WebsiteMetricsBrowserTestMixin(const WebsiteMetricsBrowserTestMixin&) =
      delete;
  WebsiteMetricsBrowserTestMixin& operator=(
      const WebsiteMetricsBrowserTestMixin&) = delete;
  ~WebsiteMetricsBrowserTestMixin() override;

  // InProcessBrowserTestMixin:
  void SetUpOnMainThread() override;

  // Creates and activates a browser window with the primary user profile.
  // Returns a pointer to the created browser instance.
  Browser* CreateBrowser();

  // Navigates to a given URL in the specified browser instance using the given
  // disposition.
  ::content::WebContents* NavigateAndWait(Browser* browser,
                                          const std::string& url,
                                          WindowOpenDisposition disposition);

  // Navigates to the given URL in the specified browser instance active tab.
  void NavigateActiveTab(Browser* browser, const std::string& url);

  // Inserts a new tab in the foreground and navigates to the given URL in the
  // specified browser instance.
  ::content::WebContents* InsertForegroundTab(Browser* browser,
                                              const std::string& url);

  // Inserts a new background tab and navigates to the given URL in the
  // specified browser instance.
  ::content::WebContents* InsertBackgroundTab(Browser* browser,
                                              const std::string& url);

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Returns the `WebsiteMetricsServiceLacros` component if initialized.
  WebsiteMetricsServiceLacros* metrics_service();
#else
  // Returns the `AppPlatformMetricsService` component if initialized.
  AppPlatformMetricsService* metrics_service();
#endif

  // Returns the `WebsiteMetrics` component if initialized.
  WebsiteMetrics* website_metrics();

 private:
#if BUILDFLAG(IS_CHROMEOS_ASH)
  raw_ptr<AppPlatformMetricsService, DanglingUntriaged>
      app_platform_metrics_service_ = nullptr;
#else
  raw_ptr<WebsiteMetricsServiceLacros, DanglingUntriaged>
      website_metrics_service_ = nullptr;
#endif
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_METRICS_WEBSITE_METRICS_BROWSER_TEST_MIXIN_H_
