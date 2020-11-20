// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_observer.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

class ExternalProtocolHandlerBrowserTest : public InProcessBrowserTest {};

// Observe that the tab is created then automatically closed.
class TabAddedRemovedObserver : public TabStripModelObserver {
 public:
  explicit TabAddedRemovedObserver(TabStripModel* tab_strip_model) {
    tab_strip_model->AddObserver(this);
  }

  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override {
    if (change.type() == TabStripModelChange::kInserted) {
      inserted_ = true;
      return;
    }
    if (change.type() == TabStripModelChange::kRemoved) {
      EXPECT_TRUE(inserted_);
      removed_ = true;
      loop_.Quit();
      return;
    }
    NOTREACHED();
  }

  void Wait() {
    if (inserted_ && removed_)
      return;
    loop_.Run();
  }

 private:
  bool inserted_ = false;
  bool removed_ = false;
  base::RunLoop loop_;
};

// Flaky on Mac: https://crbug.com/1143762:
#if defined(OS_MAC)
#define MAYBE_AutoCloseTabOnNonWebProtocolNavigation DISABLED_AutoCloseTabOnNonWebProtocolNavigation
#else
#define MAYBE_AutoCloseTabOnNonWebProtocolNavigation AutoCloseTabOnNonWebProtocolNavigation
#endif
IN_PROC_BROWSER_TEST_F(ExternalProtocolHandlerBrowserTest,
                       MAYBE_AutoCloseTabOnNonWebProtocolNavigation) {
#if defined(OS_WIN)
  // On Win 7 the protocol is registered to be handled by Chrome and thus never
  // reaches the ExternalProtocolHandler so we skip the test. For
  // more info see installer/util/shell_util.cc:GetShellIntegrationEntries
  if (base::win::GetVersion() < base::win::Version::WIN8)
    return;
#endif

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  TabAddedRemovedObserver observer(browser()->tab_strip_model());
  ASSERT_EQ(browser()->tab_strip_model()->count(), 1);
  ASSERT_TRUE(
      ExecJs(web_contents, "window.open('mailto:test@site.test', '_blank');"));
  observer.Wait();
  EXPECT_EQ(browser()->tab_strip_model()->count(), 1);
}

// Flaky on Mac: https://crbug.com/1143762:
#if defined(OS_MAC)
#define MAYBE_ProtocolLaunchEmitsConsoleLog \
  DISABLED_ProtocolLaunchEmitsConsoleLog
#else
#define MAYBE_ProtocolLaunchEmitsConsoleLog ProtocolLaunchEmitsConsoleLog
#endif
IN_PROC_BROWSER_TEST_F(ExternalProtocolHandlerBrowserTest,
                       MAYBE_ProtocolLaunchEmitsConsoleLog) {
#if defined(OS_WIN)
  // On Win 7 the protocol is registered to be handled by Chrome and thus never
  // reaches the ExternalProtocolHandler so we skip the test. For
  // more info see installer/util/shell_util.cc:GetShellIntegrationEntries
  if (base::win::GetVersion() < base::win::Version::WIN8)
    return;
#endif

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  content::WebContentsConsoleObserver observer(web_contents);
  // Wait for either "Launched external handler..." or "Failed to launch..."; the former will pass
  // the test, while the latter will fail it more quickly than waiting for a timeout.
  observer.SetPattern("*aunch*'mailto:test@site.test'*");
  ASSERT_TRUE(
      ExecJs(web_contents, "window.open('mailto:test@site.test', '_self');"));
  observer.Wait();
  ASSERT_EQ(1u, observer.messages().size());
  EXPECT_EQ("Launched external handler for 'mailto:test@site.test'.",
            observer.GetMessageAt(0u));
}

IN_PROC_BROWSER_TEST_F(ExternalProtocolHandlerBrowserTest,
                       ProtocolFailureEmitsConsoleLog) {
// Only on Mac and Windows is there a way for Chromium to know whether a
// protocol handler is registered ahead of time.
#if defined(OS_MAC) || defined(OS_WIN)
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  content::WebContentsConsoleObserver observer(web_contents);
  observer.SetPattern("Failed to launch 'does.not.exist:failure'*");
  ASSERT_TRUE(
      ExecJs(web_contents, "window.open('does.not.exist:failure', '_self');"));
  observer.Wait();
  ASSERT_EQ(1u, observer.messages().size());
#endif
}
