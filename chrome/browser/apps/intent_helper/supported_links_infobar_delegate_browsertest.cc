// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/intent_helper/supported_links_infobar_delegate.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/test/web_app_navigation_browsertest.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"

class SupportedLinksInfoBarDelegateBrowserTest
    : public web_app::WebAppNavigationBrowserTest {
 public:
  void SetUpOnMainThread() override {
    web_app::WebAppNavigationBrowserTest::SetUpOnMainThread();

    InstallTestWebApp();
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
  Browser* browser = OpenTestWebApp();
  auto* contents = browser->tab_strip_model()->GetActiveWebContents();
  apps::SupportedLinksInfoBarDelegate::MaybeShowSupportedLinksInfoBar(
      contents, test_web_app_id());

  auto* infobar = GetInfoBar(contents);
  EXPECT_TRUE(infobar);
  GetDelegate(infobar)->Accept();

  app_service_proxy()->FlushMojoCallsForTesting();

  ASSERT_TRUE(
      app_service_proxy()->PreferredApps().IsPreferredAppForSupportedLinks(
          test_web_app_id()));
}

IN_PROC_BROWSER_TEST_F(SupportedLinksInfoBarDelegateBrowserTest,
                       InfoBarNotShownForPreferredApp) {
  app_service_proxy()->SetSupportedLinksPreference(test_web_app_id());
  app_service_proxy()->FlushMojoCallsForTesting();

  Browser* browser = OpenTestWebApp();
  auto* contents = browser->tab_strip_model()->GetActiveWebContents();
  apps::SupportedLinksInfoBarDelegate::MaybeShowSupportedLinksInfoBar(
      contents, test_web_app_id());

  ASSERT_FALSE(GetInfoBar(contents));
}
