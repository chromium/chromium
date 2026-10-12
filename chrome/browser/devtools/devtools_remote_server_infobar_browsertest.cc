// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/devtools/chrome_devtools_manager_delegate.h"
#include "chrome/browser/infobars/infobar_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_delegate.h"
#include "content/public/browser/devtools_manager_delegate.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "url/gurl.h"

class DevToolsRemoteServerInfobarBrowserTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<bool> {
 protected:
  DevToolsRemoteServerInfobarBrowserTest() {
    set_exit_when_last_browser_closes(false);
    if (GetParam()) {
      feature_list_.InitAndEnableFeatureWithParameters(
          infobars::kCentralizedInfoBarFramework,
          {{"MigratedDevToolsRemoteDebugging", "true"}});
    } else {
      feature_list_.InitAndDisableFeature(
          infobars::kCentralizedInfoBarFramework);
    }
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kRemoteDebuggingPort, "0");
  }

  infobars::ContentInfoBarManager* GetInfoBarManager(
      BrowserWindowInterface* browser) {
    return infobars::ContentInfoBarManager::FromWebContents(
        browser->GetTabStripModel()->GetActiveWebContents());
  }

  void SetActiveWebSocketConnections(size_t count) {
    content::DevToolsManagerDelegate* delegate =
        ChromeDevToolsManagerDelegate::GetInstance();
    ASSERT_TRUE(delegate);
    delegate->SetActiveWebSocketConnections(count);
  }

  size_t CountRemoteDebuggingInfoBars(BrowserWindowInterface* browser) {
    size_t count = 0;
    for (infobars::InfoBar* infobar : GetInfoBarManager(browser)->infobars()) {
      if (infobar->delegate()->GetIdentifier() ==
          infobars::InfoBarDelegate::
              DEV_TOOLS_REMOTE_DEBUGGING_INFOBAR_DELEGATE) {
        ++count;
      }
    }
    return count;
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(DevToolsRemoteServerInfobarBrowserTest,
                       NoCrashWhenAllBrowsersClosedBeforeDisconnect) {
  ChromeDevToolsManagerDelegate* delegate =
      ChromeDevToolsManagerDelegate::GetInstance();
  ASSERT_TRUE(delegate);

  delegate->SetActiveWebSocketConnections(1);

  CloseBrowserSynchronously(browser());
  SetBrowser(nullptr);

  delegate->SetActiveWebSocketConnections(0);
}

IN_PROC_BROWSER_TEST_P(DevToolsRemoteServerInfobarBrowserTest,
                       AcceptAfterBrowserClosedUsesActiveBrowser) {
  ChromeDevToolsManagerDelegate* delegate =
      ChromeDevToolsManagerDelegate::GetInstance();
  ASSERT_TRUE(delegate);

  delegate->SetActiveWebSocketConnections(1);

  BrowserWindowInterface* second_browser =
      CreateBrowser(browser()->GetProfile());
  ASSERT_TRUE(second_browser);

  CloseBrowserSynchronously(browser());
  SetBrowser(second_browser);

  infobars::ContentInfoBarManager* manager = GetInfoBarManager(second_browser);
  ASSERT_EQ(1u, manager->infobars().size());

  content::TestNavigationObserver navigation_observer(
      GURL("chrome://inspect#remote-debugging"));
  navigation_observer.StartWatchingNewWebContents();

  auto* confirm =
      manager->infobars()[0]->delegate()->AsConfirmInfoBarDelegate();
  ASSERT_TRUE(confirm);

  EXPECT_TRUE(confirm->Accept());

  navigation_observer.Wait();
  EXPECT_EQ(2, second_browser->GetTabStripModel()->count());
  EXPECT_EQ(GURL("chrome://inspect#remote-debugging"),
            second_browser->GetTabStripModel()
                ->GetActiveWebContents()
                ->GetVisibleURL());
}

IN_PROC_BROWSER_TEST_P(DevToolsRemoteServerInfobarBrowserTest,
                       ShowsOnConnectionAndHidesOnDisconnect) {
  EXPECT_EQ(0u, CountRemoteDebuggingInfoBars(browser()));

  SetActiveWebSocketConnections(1);
  EXPECT_EQ(1u, CountRemoteDebuggingInfoBars(browser()));

  SetActiveWebSocketConnections(2);
  EXPECT_EQ(1u, CountRemoteDebuggingInfoBars(browser()));

  SetActiveWebSocketConnections(0);
  EXPECT_EQ(0u, CountRemoteDebuggingInfoBars(browser()));
}

IN_PROC_BROWSER_TEST_P(DevToolsRemoteServerInfobarBrowserTest,
                       AcceptClosesInfoBarAndOpensInspect) {
  SetActiveWebSocketConnections(1);
  ASSERT_EQ(1u, CountRemoteDebuggingInfoBars(browser()));

  content::TestNavigationObserver navigation_observer(
      GURL("chrome://inspect#remote-debugging"));
  navigation_observer.StartWatchingNewWebContents();

  infobars::InfoBar* infobar = GetInfoBarManager(browser())->infobars()[0];
  if (infobar->delegate()->AsConfirmInfoBarDelegate()->Accept()) {
    infobar->RemoveSelf();
  }

  navigation_observer.Wait();
  EXPECT_EQ(2, browser()->GetTabStripModel()->count());
  EXPECT_EQ(
      GURL("chrome://inspect#remote-debugging"),
      browser()->GetTabStripModel()->GetActiveWebContents()->GetVisibleURL());
  EXPECT_EQ(0u, CountRemoteDebuggingInfoBars(browser()));

  SetActiveWebSocketConnections(0);
}

IN_PROC_BROWSER_TEST_P(DevToolsRemoteServerInfobarBrowserTest,
                       ReshowsOnNextConnectionAfterDismiss) {
  SetActiveWebSocketConnections(1);
  ASSERT_EQ(1u, CountRemoteDebuggingInfoBars(browser()));

  infobars::InfoBar* infobar = GetInfoBarManager(browser())->infobars()[0];
  infobar->delegate()->InfoBarDismissed();
  infobar->RemoveSelf();
  EXPECT_EQ(0u, CountRemoteDebuggingInfoBars(browser()));

  SetActiveWebSocketConnections(2);
  EXPECT_EQ(1u, CountRemoteDebuggingInfoBars(browser()));

  SetActiveWebSocketConnections(0);
  EXPECT_EQ(0u, CountRemoteDebuggingInfoBars(browser()));
}

INSTANTIATE_TEST_SUITE_P(All,
                         DevToolsRemoteServerInfobarBrowserTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "Migrated" : "Legacy";
                         });
