// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/devtools/chrome_devtools_manager_delegate.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "url/gurl.h"

class DevToolsRemoteServerInfobarBrowserTest : public InProcessBrowserTest {
 protected:
  DevToolsRemoteServerInfobarBrowserTest() {
    set_exit_when_last_browser_closes(false);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kRemoteDebuggingPort, "0");
  }

  infobars::ContentInfoBarManager* GetInfoBarManager(Browser* browser) {
    return infobars::ContentInfoBarManager::FromWebContents(
        browser->tab_strip_model()->GetActiveWebContents());
  }
};

IN_PROC_BROWSER_TEST_F(DevToolsRemoteServerInfobarBrowserTest,
                       NoCrashWhenAllBrowsersClosedBeforeDisconnect) {
  ChromeDevToolsManagerDelegate* delegate =
      ChromeDevToolsManagerDelegate::GetInstance();
  ASSERT_TRUE(delegate);

  delegate->SetActiveWebSocketConnections(1);

  CloseBrowserSynchronously(browser());
  SetBrowser(nullptr);

  delegate->SetActiveWebSocketConnections(0);
}

IN_PROC_BROWSER_TEST_F(DevToolsRemoteServerInfobarBrowserTest,
                       AcceptAfterBrowserClosedUsesActiveBrowser) {
  ChromeDevToolsManagerDelegate* delegate =
      ChromeDevToolsManagerDelegate::GetInstance();
  ASSERT_TRUE(delegate);

  delegate->SetActiveWebSocketConnections(1);

  Browser* second_browser = CreateBrowser(browser()->profile());
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
  EXPECT_EQ(2, second_browser->tab_strip_model()->count());
  EXPECT_EQ(GURL("chrome://inspect#remote-debugging"),
            second_browser->tab_strip_model()
                ->GetActiveWebContents()
                ->GetVisibleURL());
}
