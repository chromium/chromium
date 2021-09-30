// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/navigation_handle_observer.h"
#include "content/public/test/test_navigation_observer.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

namespace {

// Navigate to an external protocol from an iframe. Returns whether the
// navigation was allowed by sandbox. `iframe_sandbox` is used to define the
// iframe's sandbox flag.
bool AllowedBySandbox(content::WebContents* web_content,
                      std::string iframe_sandbox,
                      bool has_user_gesture = false) {
  EXPECT_TRUE(content::ExecJs(
      web_content, "const iframe = document.createElement('iframe');" +
                       iframe_sandbox +
                       "iframe.src = '/empty.html';"
                       "document.body.appendChild(iframe)"));
  EXPECT_TRUE(content::WaitForLoadStop(web_content));

  EXPECT_EQ(2u, web_content->GetAllFrames().size());
  content::RenderFrameHost* child_document = web_content->GetAllFrames()[1];

  content::WebContentsConsoleObserver observer(web_content);
  observer.SetPattern("*external*");

  EXPECT_TRUE(content::ExecJs(
      child_document, "location.href = 'mailto:test@site.test';",
      has_user_gesture ? content::EXECUTE_SCRIPT_DEFAULT_OPTIONS
                       : content::EXECUTE_SCRIPT_NO_USER_GESTURE));

  observer.Wait();
  std::string message = observer.GetMessageAt(0u);
  const char allowed_msg[] =
      "Launched external handler for 'mailto:test@site.test'.";
  const char blocked_msg[] =
      "Navigation to external protocol blocked by sandbox.";

  EXPECT_TRUE(message == allowed_msg || message == blocked_msg);
  return message == allowed_msg;
}

}  // namespace

class ExternalProtocolHandlerBrowserTest : public InProcessBrowserTest {
 public:
  content::WebContents* web_content() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }
};

class ExternalProtocolHandlerSandboxBrowserTest
    : public ExternalProtocolHandlerBrowserTest {
 public:
  ExternalProtocolHandlerSandboxBrowserTest() {
    features_.InitAndEnableFeature(features::kSandboxExternalProtocolBlocked);
  }

 private:
  base::test::ScopedFeatureList features_;
};

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

  TabAddedRemovedObserver observer(browser()->tab_strip_model());
  ASSERT_EQ(browser()->tab_strip_model()->count(), 1);
  ASSERT_TRUE(
      ExecJs(web_content(), "window.open('mailto:test@site.test', '_blank');"));
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

  content::WebContentsConsoleObserver observer(web_content());
  // Wait for either "Launched external handler..." or "Failed to launch..."; the former will pass
  // the test, while the latter will fail it more quickly than waiting for a timeout.
  observer.SetPattern("*aunch*'mailto:test@site.test'*");
  ASSERT_TRUE(
      ExecJs(web_content(), "window.open('mailto:test@site.test', '_self');"));
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
  content::WebContentsConsoleObserver observer(web_content());
  observer.SetPattern("Failed to launch 'does.not.exist:failure'*");
  ASSERT_TRUE(
      ExecJs(web_content(), "window.open('does.not.exist:failure', '_self');"));
  observer.Wait();
  ASSERT_EQ(1u, observer.messages().size());
#endif
}

// External protocol are allowed when iframe's sandbox contains one of:
// - allow-popups
// - allow-top-navigation
// - allow-top-navigation-by-user-activation + UserGesture
IN_PROC_BROWSER_TEST_F(ExternalProtocolHandlerSandboxBrowserTest, Sandbox) {
  EXPECT_TRUE(AllowedBySandbox(web_content(), ""));
}

IN_PROC_BROWSER_TEST_F(ExternalProtocolHandlerSandboxBrowserTest, SandboxAll) {
  EXPECT_FALSE(
      AllowedBySandbox(web_content(), "iframe.sandbox = 'allow-scripts';"));
}

IN_PROC_BROWSER_TEST_F(ExternalProtocolHandlerBrowserTest, SandboxAll) {
  EXPECT_TRUE(
      AllowedBySandbox(web_content(), "iframe.sandbox = 'allow-scripts';"));
}

IN_PROC_BROWSER_TEST_F(ExternalProtocolHandlerSandboxBrowserTest,
                       SandboxAllowPopups) {
  EXPECT_TRUE(AllowedBySandbox(
      web_content(), "iframe.sandbox = 'allow-scripts allow-popups';"));
}

IN_PROC_BROWSER_TEST_F(ExternalProtocolHandlerSandboxBrowserTest,
                       SandboxAllowTopNavigation) {
  EXPECT_TRUE(AllowedBySandbox(
      web_content(), "iframe.sandbox = 'allow-scripts allow-top-navigation';"));
}

IN_PROC_BROWSER_TEST_F(ExternalProtocolHandlerSandboxBrowserTest,
                       SandboxAllowTopNavigationByUserActivation) {
  EXPECT_FALSE(AllowedBySandbox(web_content(),
                                "iframe.sandbox = 'allow-scripts "
                                "allow-top-navigation-by-user-activation';",
                                /*user-gesture=*/false));
}

IN_PROC_BROWSER_TEST_F(ExternalProtocolHandlerSandboxBrowserTest,
                       SandboxAllowTopNavigationByUserActivationWithGesture) {
  EXPECT_TRUE(AllowedBySandbox(web_content(),
                               "iframe.sandbox = 'allow-scripts "
                               "allow-top-navigation-by-user-activation';",
                               /*user-gesture=*/true));
}
