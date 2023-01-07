// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/intent_helper/supported_links_infobar_delegate.h"

#include <memory>

#include "base/feature_list.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/intent_helper/metrics/intent_handling_metrics.h"
#include "chrome/browser/apps/intent_helper/preferred_apps_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/web_applications/test/web_app_navigation_browsertest.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "components/services/app_service/public/cpp/features.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

class SupportedLinksInfoBarDelegateBrowserTest
    : public web_app::WebAppNavigationBrowserTest {
 public:
  void SetUpOnMainThread() override {
    web_app::WebAppNavigationBrowserTest::SetUpOnMainThread();

    InstallTestWebApp();
  }

  void TearDownOnMainThread() override {
    web_app::test::UninstallWebApp(profile(), test_web_app_id());
    web_app::WebAppNavigationBrowserTest::TearDownOnMainThread();
  }

  infobars::InfoBar* GetInfoBar(content::WebContents* contents) {
    auto* manager = infobars::ContentInfoBarManager::FromWebContents(contents);

    if (manager->infobar_count() != 1)
      return nullptr;
    return manager->infobar_at(0);
  }

  ConfirmInfoBarDelegate* GetDelegate(infobars::InfoBar* infobar) {
    return infobar->delegate()->AsConfirmInfoBarDelegate();
  }

  apps::AppServiceProxy* app_service_proxy() {
    return apps::AppServiceProxyFactory::GetForProfile(profile());
  }
};

IN_PROC_BROWSER_TEST_F(SupportedLinksInfoBarDelegateBrowserTest,
                       AcceptInfoBarChangesSupportedLinks) {
  base::HistogramTester histogram_tester;
  Browser* browser = OpenTestWebApp();
  auto* contents = browser->tab_strip_model()->GetActiveWebContents();
  apps::SupportedLinksInfoBarDelegate::MaybeShowSupportedLinksInfoBar(
      contents, test_web_app_id());

  apps_util::PreferredAppUpdateWaiter update_waiter(
      app_service_proxy()->PreferredAppsList(), test_web_app_id());

  auto* infobar = GetInfoBar(contents);
  EXPECT_TRUE(infobar);
  GetDelegate(infobar)->Accept();

  update_waiter.Wait();

  ASSERT_TRUE(
      app_service_proxy()->PreferredAppsList().IsPreferredAppForSupportedLinks(
          test_web_app_id()));

  histogram_tester.ExpectBucketCount(
      "ChromeOS.Intents.LinkCapturingEvent2.WebApp",
      apps::IntentHandlingMetrics::LinkCapturingEvent::kSettingsChanged, 1);
  histogram_tester.ExpectBucketCount(
      "ChromeOS.Intents.LinkCapturingEvent2",
      apps::IntentHandlingMetrics::LinkCapturingEvent::kSettingsChanged, 1);
}

IN_PROC_BROWSER_TEST_F(SupportedLinksInfoBarDelegateBrowserTest,
                       InfoBarNotShownForPreferredApp) {
  apps_util::SetSupportedLinksPreferenceAndWait(profile(), test_web_app_id());

  Browser* browser = OpenTestWebApp();
  auto* contents = browser->tab_strip_model()->GetActiveWebContents();
  apps::SupportedLinksInfoBarDelegate::MaybeShowSupportedLinksInfoBar(
      contents, test_web_app_id());

  ASSERT_FALSE(GetInfoBar(contents));
}

IN_PROC_BROWSER_TEST_F(SupportedLinksInfoBarDelegateBrowserTest,
                       InfoBarNotShownAfterDismiss) {
  {
    auto* browser = OpenTestWebApp();
    auto* contents = browser->tab_strip_model()->GetActiveWebContents();
    apps::SupportedLinksInfoBarDelegate::MaybeShowSupportedLinksInfoBar(
        contents, test_web_app_id());

    auto* infobar = GetInfoBar(contents);
    GetDelegate(infobar)->Cancel();
  }

  {
    auto* browser = OpenTestWebApp();
    auto* contents = browser->tab_strip_model()->GetActiveWebContents();

    apps::SupportedLinksInfoBarDelegate::MaybeShowSupportedLinksInfoBar(
        contents, test_web_app_id());
    ASSERT_FALSE(GetInfoBar(contents));
  }
}

IN_PROC_BROWSER_TEST_F(SupportedLinksInfoBarDelegateBrowserTest,
                       InfoBarNotShownAfterIgnored) {
  for (int i = 0; i < 3; i++) {
    auto* browser = OpenTestWebApp();
    auto* contents = browser->tab_strip_model()->GetActiveWebContents();
    apps::SupportedLinksInfoBarDelegate::MaybeShowSupportedLinksInfoBar(
        contents, test_web_app_id());

    ASSERT_TRUE(GetInfoBar(contents));
    chrome::CloseTab(browser);
  }

  {
    auto* browser = OpenTestWebApp();
    auto* contents = browser->tab_strip_model()->GetActiveWebContents();

    apps::SupportedLinksInfoBarDelegate::MaybeShowSupportedLinksInfoBar(
        contents, test_web_app_id());
    ASSERT_FALSE(GetInfoBar(contents));
  }
}

IN_PROC_BROWSER_TEST_F(SupportedLinksInfoBarDelegateBrowserTest,
                       InfoBarDismissedWhenOpenedInChrome) {
  Browser* browser = OpenTestWebApp();
  auto* contents = browser->tab_strip_model()->GetActiveWebContents();
  apps::SupportedLinksInfoBarDelegate::MaybeShowSupportedLinksInfoBar(
      contents, test_web_app_id());
  ASSERT_TRUE(GetInfoBar(contents));

  chrome::OpenInChrome(browser);
  ASSERT_FALSE(GetInfoBar(contents));
}
