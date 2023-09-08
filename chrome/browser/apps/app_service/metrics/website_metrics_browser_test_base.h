// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_METRICS_WEBSITE_METRICS_BROWSER_TEST_BASE_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_METRICS_WEBSITE_METRICS_BROWSER_TEST_BASE_H_

#include "base/command_line.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/window_open_disposition.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/apps/app_service/metrics/app_platform_metrics_service.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/apps/app_service/metrics/website_metrics_service_lacros.h"
#endif

namespace apps {

// Base class with basic browser test environment setup for the `WebsiteMetrics`
// component. Also includes relevant helpers that can be used by browser tests
// for simulating browser window/tab interaction.
class WebsiteMetricsBrowserTestBase : public InProcessBrowserTest {
 protected:
  // InProcessBrowserTest:
  void SetUpOnMainThread() override;
  void SetUpCommandLine(base::CommandLine* command_line) override;

  // Creates and activates a browser window. Returns a pointer to the browser
  // instance.
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
  // Returns the `WebsiteMetricsServiceLacros` component.
  WebsiteMetricsServiceLacros* metrics_service();
#else
  // Returns the `AppPlatformMetricsService` component.
  AppPlatformMetricsService* metrics_service();
#endif

  // Returns the initialized `WebsiteMetrics` component.
  WebsiteMetrics* website_metrics();

  // Returns a pointer to the main user profile that was used to initialize
  // website metric components.
  Profile* profile();

 private:
#if BUILDFLAG(IS_CHROMEOS_ASH)
  raw_ptr<AppPlatformMetricsService, DanglingUntriaged | ExperimentalAsh>
      app_platform_metrics_service_ = nullptr;
#else
  raw_ptr<WebsiteMetricsServiceLacros> website_metrics_service_ = nullptr;
#endif
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_METRICS_WEBSITE_METRICS_BROWSER_TEST_BASE_H_
