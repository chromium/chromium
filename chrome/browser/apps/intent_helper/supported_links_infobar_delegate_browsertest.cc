// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/intent_helper/supported_links_infobar_delegate.h"

#include <memory>

#include "base/run_loop.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/intent_helper/metrics/intent_handling_metrics.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/web_applications/test/web_app_navigation_browsertest.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "components/services/app_service/public/cpp/preferred_apps_list_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

class SupportedLinksInfoBarDelegateBrowserTest
    : public web_app::WebAppNavigationBrowserTest,
      public apps::PreferredAppsListHandle::Observer {
 public:
  void SetUpOnMainThread() override {
    web_app::WebAppNavigationBrowserTest::SetUpOnMainThread();

    InstallTestWebApp();
    app_service_proxy()->PreferredAppsList().AddObserver(this);
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

  // apps::PreferredAppsListHandle::Observer:
  void WaitForPreferredAppUpdate() {
    wait_run_loop_ = std::make_unique<base::RunLoop>();
    wait_run_loop_->Run();
  }

  void OnPreferredAppChanged(const std::string& app_id,
                             bool is_preferred_app) override {
    if (wait_run_loop_ && wait_run_loop_->running() &&
        app_id == test_web_app_id()) {
      wait_run_loop_->Quit();
    }
  }

  void OnPreferredAppsListWillBeDestroyed(
      apps::PreferredAppsListHandle* handle) override {
    handle->RemoveObserver(this);
  }

 private:
  std::unique_ptr<base::RunLoop> wait_run_loop_;
};

IN_PROC_BROWSER_TEST_F(SupportedLinksInfoBarDelegateBrowserTest,
                       AcceptInfoBarChangesSupportedLinks) {
  if (!apps::SupportedLinksInfoBarDelegate::
          IsSetSupportedLinksPreferenceSupported()) {
    GTEST_SKIP() << "Ash version not supported";
  }

  base::HistogramTester histogram_tester;
  Browser* browser = OpenTestWebApp();
  auto* contents = browser->tab_strip_model()->GetActiveWebContents();
  apps::SupportedLinksInfoBarDelegate::MaybeShowSupportedLinksInfoBar(
      contents, test_web_app_id());

  auto* infobar = GetInfoBar(contents);
  EXPECT_TRUE(infobar);
  GetDelegate(infobar)->Accept();

  WaitForPreferredAppUpdate();

  ASSERT_TRUE(
      app_service_proxy()->PreferredAppsList().IsPreferredAppForSupportedLinks(
          test_web_app_id()));

  histogram_tester.ExpectBucketCount(
      "ChromeOS.Intents.LinkCapturingEvent.WebApp",
      apps::IntentHandlingMetrics::LinkCapturingEvent::kSettingsChanged, 1);
  histogram_tester.ExpectBucketCount(
      "ChromeOS.Intents.LinkCapturingEvent",
      apps::IntentHandlingMetrics::LinkCapturingEvent::kSettingsChanged, 1);
}

IN_PROC_BROWSER_TEST_F(SupportedLinksInfoBarDelegateBrowserTest,
                       InfoBarNotShownForPreferredApp) {
  if (!apps::SupportedLinksInfoBarDelegate::
          IsSetSupportedLinksPreferenceSupported()) {
    GTEST_SKIP() << "Ash version not supported";
  }

  app_service_proxy()->SetSupportedLinksPreference(test_web_app_id());
  WaitForPreferredAppUpdate();

  Browser* browser = OpenTestWebApp();
  auto* contents = browser->tab_strip_model()->GetActiveWebContents();
  apps::SupportedLinksInfoBarDelegate::MaybeShowSupportedLinksInfoBar(
      contents, test_web_app_id());

  ASSERT_FALSE(GetInfoBar(contents));
}

IN_PROC_BROWSER_TEST_F(SupportedLinksInfoBarDelegateBrowserTest,
                       InfoBarNotShownAfterDismiss) {
  if (!apps::SupportedLinksInfoBarDelegate::
          IsSetSupportedLinksPreferenceSupported()) {
    GTEST_SKIP() << "Ash version not supported";
  }

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
  if (!apps::SupportedLinksInfoBarDelegate::
          IsSetSupportedLinksPreferenceSupported()) {
    GTEST_SKIP() << "Ash version not supported";
  }

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
