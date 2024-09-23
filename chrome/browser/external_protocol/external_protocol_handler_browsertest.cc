// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/external_protocol/external_protocol_handler.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/navigation_handle_observer.h"
#include "content/public/test/test_navigation_observer.h"

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

  void SetUpOnMainThread() override {
    ExternalProtocolHandlerBrowserTest::SetUpOnMainThread();
    AllowCustomProtocol();
  }

  // On Linux, external protocols calls are delegated to the OS without
  // additional security check. On Windows and Mac (possibly more), it runs
  // additional security check and ask the user, without displaying a message in
  // the console. Adopt Linux's behavior for the purpose of this test suite.
  void AllowCustomProtocol() {
    base::Value::List allow_list;
    allow_list.Append("custom:*");
    browser()->profile()->GetPrefs()->Set(policy::policy_prefs::kUrlAllowlist,
                                          base::Value(std::move(allow_list)));
  }

  content::RenderFrameHost* CreateIFrame(content::RenderFrameHost* document,
                                         std::string iframe_sandbox) {
    EXPECT_TRUE(content::ExecJs(
        document, "const iframe = document.createElement('iframe');" +
                      iframe_sandbox +
                      "iframe.src = '/empty.html';"
                      "document.body.appendChild(iframe)"));

    EXPECT_TRUE(content::WaitForLoadStop(web_content()));
    return ChildFrameAt(document, 0);
  }

  // Navigate to an external protocol from an iframe or fenced frame. Returns
  // whether the navigation was allowed by sandbox. `sandbox` is used to define
  // the sub frame's sandbox flag.
  bool AllowedBySandbox(content::RenderFrameHost* child_document,
                        bool has_user_gesture = false) {
    EXPECT_TRUE(child_document);

    // When not blocked by sandbox, Linux displays |allowed_msg_1| and
    // Windows/Mac |allowed_msg_2|. That's because Linux do not provide API to
    // query for the supported protocols before launching the external
    // application.
    const char allowed_msg_1[] =
        "Launched external handler for 'custom:custom'.";
    const char allowed_msg_2[] =
        "Failed to launch 'custom:custom' because the scheme does not have a "
        "registered handler.";
    const char blocked_msg[] =
        "Navigation to external protocol blocked by sandbox, because it "
        "doesn't contain any of: "
        "'allow-top-navigation-to-custom-protocols', "
        "'allow-top-navigation-by-user-activation', "
        "'allow-top-navigation', or "
        "'allow-popups'. See "
        "https://chromestatus.com/feature/5680742077038592 and "
        "https://chromeenterprise.google/policies/"
        "#SandboxExternalProtocolBlocked";
    content::WebContentsConsoleObserver observer(web_content());
    observer.SetFilter(base::BindLambdaForTesting(
        [&](const content::WebContentsConsoleObserver::Message& message) {
          std::string value = base::UTF16ToUTF8(message.message);
          return value == allowed_msg_1 || value == allowed_msg_2 ||
                 value == blocked_msg;
        }));

    EXPECT_TRUE(content::ExecJs(
        child_document, "location.href = 'custom:custom';",
        has_user_gesture ? content::EXECUTE_SCRIPT_DEFAULT_OPTIONS
                         : content::EXECUTE_SCRIPT_NO_USER_GESTURE));

    EXPECT_TRUE(observer.Wait());
    std::string message = observer.GetMessageAt(0u);

    return message == allowed_msg_1 || message == allowed_msg_2;
  }

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
    NOTREACHED_IN_MIGRATION();
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
#if BUILDFLAG(IS_MAC)
#define MAYBE_AutoCloseTabOnNonWebProtocolNavigation DISABLED_AutoCloseTabOnNonWebProtocolNavigation
#else
#define MAYBE_AutoCloseTabOnNonWebProtocolNavigation AutoCloseTabOnNonWebProtocolNavigation
#endif
IN_PROC_BROWSER_TEST_F(ExternalProtocolHandlerBrowserTest,
                       MAYBE_AutoCloseTabOnNonWebProtocolNavigation) {
  TabAddedRemovedObserver observer(browser()->tab_strip_model());
  ASSERT_EQ(browser()->tab_strip_model()->count(), 1);
  ASSERT_TRUE(
      ExecJs(web_content(), "window.open('mailto:test@site.test', '_blank');"));
  observer.Wait();
  EXPECT_EQ(browser()->tab_strip_model()->count(), 1);
}

// Flaky on Mac: https://crbug.com/1143762:
#if BUILDFLAG(IS_MAC)
#define MAYBE_ProtocolLaunchEmitsConsoleLog \
  DISABLED_ProtocolLaunchEmitsConsoleLog
#else
#define MAYBE_ProtocolLaunchEmitsConsoleLog ProtocolLaunchEmitsConsoleLog
#endif
IN_PROC_BROWSER_TEST_F(ExternalProtocolHandlerBrowserTest,
                       MAYBE_ProtocolLaunchEmitsConsoleLog) {
  content::WebContentsConsoleObserver observer(web_content());
  // Wait for either "Launched external handler..." or "Failed to launch..."; the former will pass
  // the test, while the latter will fail it more quickly than waiting for a timeout.
  observer.SetPattern("*aunch*'mailto:test@site.test'*");
  ASSERT_TRUE(
      ExecJs(web_content(), "window.open('mailto:test@site.test', '_self');"));
  ASSERT_TRUE(observer.Wait());
  ASSERT_EQ(1u, observer.messages().size());
  EXPECT_EQ("Launched external handler for 'mailto:test@site.test'.",
            observer.GetMessageAt(0u));
}

IN_PROC_BROWSER_TEST_F(ExternalProtocolHandlerBrowserTest,
                       ProtocolFailureEmitsConsoleLog) {
// Only on Mac and Windows is there a way for Chromium to know whether a
// protocol handler is registered ahead of time.
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  content::WebContentsConsoleObserver observer(web_content());
  observer.SetPattern("Failed to launch 'does.not.exist:failure'*");
  ASSERT_TRUE(
      ExecJs(web_content(), "window.open('does.not.exist:failure', '_self');"));
  ASSERT_TRUE(observer.Wait());
  ASSERT_EQ(1u, observer.messages().size());
#endif
}

// External protocol are allowed when iframe's sandbox contains one of:
// - allow-popups
// - allow-top-navigation
// - allow-top-navigation-by-user-activation + UserGesture
IN_PROC_BROWSER_TEST_F(ExternalProtocolHandlerSandboxBrowserTest, Sandbox) {
  EXPECT_TRUE(
      AllowedBySandbox(CreateIFrame(web_content()->GetPrimaryMainFrame(), "")));
}

IN_PROC_BROWSER_TEST_F(ExternalProtocolHandlerSandboxBrowserTest, SandboxAll) {
  EXPECT_FALSE(
      AllowedBySandbox(CreateIFrame(web_content()->GetPrimaryMainFrame(),
                                    "iframe.sandbox = 'allow-scripts';")));
}

IN_PROC_BROWSER_TEST_F(ExternalProtocolHandlerSandboxBrowserTest,
                       SandboxAllowPopups) {
  EXPECT_TRUE(AllowedBySandbox(
      CreateIFrame(web_content()->GetPrimaryMainFrame(),
                   "iframe.sandbox = 'allow-scripts allow-popups';")));
}

IN_PROC_BROWSER_TEST_F(ExternalProtocolHandlerSandboxBrowserTest,
                       SandboxAllowTopNavigationToCustomProtocols) {
  EXPECT_TRUE(AllowedBySandbox(
      CreateIFrame(web_content()->GetPrimaryMainFrame(),
                   "iframe.sandbox = 'allow-scripts "
                   "allow-top-navigation-to-custom-protocols';")));
}

IN_PROC_BROWSER_TEST_F(ExternalProtocolHandlerSandboxBrowserTest,
                       SandboxAllowTopNavigation) {
  EXPECT_TRUE(AllowedBySandbox(
      CreateIFrame(web_content()->GetPrimaryMainFrame(),
                   "iframe.sandbox = 'allow-scripts allow-top-navigation';")));
}

IN_PROC_BROWSER_TEST_F(ExternalProtocolHandlerSandboxBrowserTest,
                       SandboxAllowTopNavigationByUserActivation) {
  EXPECT_FALSE(AllowedBySandbox(
      CreateIFrame(web_content()->GetPrimaryMainFrame(),
                   "iframe.sandbox = 'allow-scripts "
                   "allow-top-navigation-by-user-activation';"),
      /*user-gesture=*/false));
}

IN_PROC_BROWSER_TEST_F(ExternalProtocolHandlerSandboxBrowserTest,
                       SandboxAllowTopNavigationByUserActivationWithGesture) {
  EXPECT_TRUE(AllowedBySandbox(
      CreateIFrame(web_content()->GetPrimaryMainFrame(),
                   "iframe.sandbox = 'allow-scripts "
                   "allow-top-navigation-by-user-activation';"),
      /*user-gesture=*/true));
}

namespace {

class AlwaysBlockedExternalProtocolHandlerDelegate
    : public ExternalProtocolHandler::Delegate {
 public:
  AlwaysBlockedExternalProtocolHandlerDelegate() {
    ExternalProtocolHandler::SetDelegateForTesting(this);
  }
  ~AlwaysBlockedExternalProtocolHandlerDelegate() override {
    ExternalProtocolHandler::SetDelegateForTesting(nullptr);
  }

  scoped_refptr<shell_integration::DefaultSchemeClientWorker> CreateShellWorker(
      const GURL& url) override {
    NOTREACHED_IN_MIGRATION();
    return nullptr;
  }
  ExternalProtocolHandler::BlockState GetBlockState(const std::string& scheme,
                                                    Profile* profile) override {
    return ExternalProtocolHandler::BLOCK;
  }
  void BlockRequest() override {}
  void RunExternalProtocolDialog(
      const GURL& url,
      content::WebContents* web_contents,
      ui::PageTransition page_transition,
      bool has_user_gesture,
      const std::optional<url::Origin>& initiating_origin,
      const std::u16string& program_name) override {
    NOTREACHED_IN_MIGRATION();
  }
  void LaunchUrlWithoutSecurityCheck(
      const GURL& url,
      content::WebContents* web_contents) override {
    NOTREACHED_IN_MIGRATION();
  }
  void FinishedProcessingCheck() override {}
};

}  // namespace

// Tests (by forcing a particular scheme to be blocked, regardless of platform)
// that the console message is attributed to a subframe if one was responsible.
IN_PROC_BROWSER_TEST_F(ExternalProtocolHandlerBrowserTest,
                       ProtocolLaunchEmitsConsoleLogInCorrectFrame) {
  AlwaysBlockedExternalProtocolHandlerDelegate always_blocked;
  content::RenderFrameHost* main_rfh = ui_test_utils::NavigateToURL(
      browser(), GURL("data:text/html,<iframe srcdoc=\"Hello!\"></iframe>"));
  ASSERT_TRUE(main_rfh);
  content::RenderFrameHost* originating_rfh = ChildFrameAt(main_rfh, 0);

  content::WebContentsConsoleObserver observer(
      content::WebContents::FromRenderFrameHost(originating_rfh));
  observer.SetPattern("*aunch*'willfailtolaunch://foo'*");
  observer.SetFilter(base::BindRepeating(
      [](content::RenderFrameHost* expected_rfh,
         const content::WebContentsConsoleObserver::Message& message) {
        return message.source_frame == expected_rfh;
      },
      originating_rfh));

  ASSERT_TRUE(
      ExecJs(originating_rfh, "location.href = 'willfailtolaunch://foo';"));
  ASSERT_TRUE(observer.Wait());
  ASSERT_EQ(1u, observer.messages().size());
  EXPECT_EQ("Not allowed to launch 'willfailtolaunch://foo'.",
            observer.GetMessageAt(0u));
}

class ExternalProtocolHandlerSandboxFencedFrameBrowserTest
    : public ExternalProtocolHandlerSandboxBrowserTest {
 public:
  void SetUpOnMainThread() override {
    ExternalProtocolHandlerSandboxBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  content::RenderFrameHost* CreateFencedFrame() {
    return fenced_frame_test_helper().CreateFencedFrame(
        web_content()->GetPrimaryMainFrame(),
        embedded_test_server()->GetURL("/fenced_frames/title1.html"));
  }

  content::test::FencedFrameTestHelper& fenced_frame_test_helper() {
    return fenced_frame_test_helper_;
  }

  content::test::FencedFrameTestHelper fenced_frame_test_helper_;
};

IN_PROC_BROWSER_TEST_F(ExternalProtocolHandlerSandboxFencedFrameBrowserTest,
                       SandboxAllWithoutGesture) {
  EXPECT_TRUE(AllowedBySandbox(CreateFencedFrame(), /*user-gesture=*/false));
}

IN_PROC_BROWSER_TEST_F(ExternalProtocolHandlerSandboxFencedFrameBrowserTest,
                       SandboxAllWithGesture) {
  EXPECT_TRUE(AllowedBySandbox(CreateFencedFrame(), /*user-gesture=*/true));
}

IN_PROC_BROWSER_TEST_F(ExternalProtocolHandlerSandboxFencedFrameBrowserTest,
                       SandboxInFencedFrame) {
  EXPECT_TRUE(AllowedBySandbox(CreateIFrame(CreateFencedFrame(), "")));
}

IN_PROC_BROWSER_TEST_F(ExternalProtocolHandlerSandboxFencedFrameBrowserTest,
                       SandboxAllInFencedFrame) {
  EXPECT_FALSE(AllowedBySandbox(
      CreateIFrame(CreateFencedFrame(), "iframe.sandbox = 'allow-scripts';")));
}

IN_PROC_BROWSER_TEST_F(ExternalProtocolHandlerSandboxFencedFrameBrowserTest,
                       SandboxAllowPopupsInFencedFrame) {
  EXPECT_TRUE(AllowedBySandbox(CreateIFrame(
      CreateFencedFrame(), "iframe.sandbox = 'allow-scripts allow-popups';")));
}

IN_PROC_BROWSER_TEST_F(ExternalProtocolHandlerSandboxFencedFrameBrowserTest,
                       SandboxAllowTopNavigationInFencedFrame) {
  EXPECT_TRUE(AllowedBySandbox(
      CreateIFrame(CreateFencedFrame(),
                   "iframe.sandbox = 'allow-scripts allow-top-navigation';")));
}

IN_PROC_BROWSER_TEST_F(
    ExternalProtocolHandlerSandboxFencedFrameBrowserTest,
    SandboxAllowTopNavigationToCustomProtocolsInFencedFrame) {
  EXPECT_TRUE(AllowedBySandbox(
      CreateIFrame(CreateFencedFrame(),
                   "iframe.sandbox = 'allow-scripts "
                   "allow-top-navigation-to-custom-protocols';")));
}

IN_PROC_BROWSER_TEST_F(ExternalProtocolHandlerSandboxFencedFrameBrowserTest,
                       SandboxAllowTopNavigationByUserActivationInFencedFrame) {
  EXPECT_FALSE(AllowedBySandbox(
      CreateIFrame(CreateFencedFrame(),
                   "iframe.sandbox = 'allow-scripts "
                   "allow-top-navigation-by-user-activation';"),
      /*user-gesture=*/false));
}

IN_PROC_BROWSER_TEST_F(
    ExternalProtocolHandlerSandboxFencedFrameBrowserTest,
    SandboxAllowTopNavigationByUserActivationWithGestureInFencedFrame) {
  EXPECT_TRUE(AllowedBySandbox(
      CreateIFrame(web_content()->GetPrimaryMainFrame(),
                   "iframe.sandbox = 'allow-scripts "
                   "allow-top-navigation-by-user-activation';"),
      /*user-gesture=*/true));
}
