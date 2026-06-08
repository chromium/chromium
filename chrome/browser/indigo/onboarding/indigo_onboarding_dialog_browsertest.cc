// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/indigo/onboarding/indigo_onboarding_dialog.h"

#include "base/functional/callback_helpers.h"
#include "base/strings/escape.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/tabs/public/tab_dialog_manager.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/test/ui_controls.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace indigo {
namespace {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kDialogWebContentsId);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewTabId);

MATCHER_P2(SizeIsInRange, min_size, max_size, "") {
  return arg.width() >= min_size.width() && arg.width() <= max_size.width() &&
         arg.height() >= min_size.height() && arg.height() <= max_size.height();
}

class IndigoOnboardingDialogBrowserTest : public InteractiveBrowserTest {
 public:
  IndigoOnboardingDialogBrowserTest() {
    feature_list_.InitAndEnableFeature(features::kIndigo);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InteractiveBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kIsolateOrigins,
                                    "http://isolated.com");
  }

 protected:
  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void OpenDialog(tabs::TabInterface& tab, const GURL& url) {
    dialog_ = IndigoOnboardingDialog::Show(
        tab, url,
        base::BindLambdaForTesting([&](const OnboardingResult& result) {
          closed_ = true;
          last_result_ = result;
          dialog_.reset();
        }));
  }

  bool WasDialogClosed() const { return closed_; }

  std::unique_ptr<IndigoOnboardingDialog> dialog_;
  bool closed_ = false;
  OnboardingResult last_result_;
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(IndigoOnboardingDialogBrowserTest, ShowAndClose) {
  tabs::TabInterface* tab = browser()->GetActiveTabInterface();
  ASSERT_TRUE(tab);

  const GURL example_url("https://www.example.com/");
  RunTestSequence(
      Do([&]() { OpenDialog(*tab, example_url); }),
      WaitForShow(IndigoOnboardingDialog::kWebViewId),
      CheckView(
          IndigoOnboardingDialog::kWebViewId,
          [](views::WebView* web_view) { return web_view->GetPreferredSize(); },
          SizeIsInRange(gfx::Size(448, 100), gfx::Size(448, 960))),
      Do([&]() { dialog_->Close(); }),
      WaitForHide(IndigoOnboardingDialog::kWebViewId),
      Check([&]() { return WasDialogClosed(); }));
}

IN_PROC_BROWSER_TEST_F(IndigoOnboardingDialogBrowserTest, SendsRequestHeader) {
  tabs::TabInterface* tab = browser()->GetActiveTabInterface();
  ASSERT_TRUE(tab);

  const GURL onboarding_url("about:blank");
  RunTestSequence(Do([&]() { OpenDialog(*tab, onboarding_url); }),
                  WaitForShow(IndigoOnboardingDialog::kWebViewId),
                  InstrumentNonTabWebView(kDialogWebContentsId,
                                          IndigoOnboardingDialog::kWebViewId),
                  WaitForWebContentsReady(kDialogWebContentsId, onboarding_url),
                  CheckView(
                      IndigoOnboardingDialog::kWebViewId,
                      [](views::WebView* web_view) {
                        content::NavigationEntry* entry =
                            web_view->GetWebContents()
                                ->GetController()
                                .GetLastCommittedEntry();
                        return entry ? entry->GetExtraHeaders() : std::string();
                      },
                      testing::HasSubstr("X-Chrome-Onboarding: ?1")));
}

IN_PROC_BROWSER_TEST_F(IndigoOnboardingDialogBrowserTest, JSWindowClose) {
  tabs::TabInterface* tab = browser()->GetActiveTabInterface();
  ASSERT_TRUE(tab);

  const GURL onboarding_url = embedded_test_server()->GetURL("/empty.html");
  RunTestSequence(Do([&]() { OpenDialog(*tab, onboarding_url); }),
                  WaitForShow(IndigoOnboardingDialog::kWebViewId),
                  InstrumentNonTabWebView(kDialogWebContentsId,
                                          IndigoOnboardingDialog::kWebViewId),
                  WaitForWebContentsReady(kDialogWebContentsId, onboarding_url),
                  ExecuteJs(kDialogWebContentsId, "() => window.close()",
                            ExecuteJsMode::kFireAndForget),
                  WaitForHide(IndigoOnboardingDialog::kWebViewId),
                  Check([&]() { return WasDialogClosed(); }));
}

// Test clicking a link with target="_blank". This verifies the
// WebContentsDelegate::AddNewContents implementation.
IN_PROC_BROWSER_TEST_F(IndigoOnboardingDialogBrowserTest,
                       ClickTargetBlankLink) {
  tabs::TabInterface* tab = browser()->GetActiveTabInterface();
  ASSERT_TRUE(tab);

  const GURL onboarding_url =
      embedded_test_server()->GetURL("/link_new_page.html");

  RunTestSequence(
      Do([&]() { OpenDialog(*tab, onboarding_url); }),
      WaitForShow(IndigoOnboardingDialog::kWebViewId),
      InstrumentNonTabWebView(kDialogWebContentsId,
                              IndigoOnboardingDialog::kWebViewId),
      WaitForWebContentsReady(kDialogWebContentsId, onboarding_url),
      InstrumentNextTab(kNewTabId),
      ClickElement(kDialogWebContentsId, DeepQuery{"#new-page-link"})
          .SetMustRemainVisible(false),
      WaitForWebContentsReady(
          kNewTabId, embedded_test_server()->GetURL("/link_new_page.html")));
}

// Test middle-clicking a link. This verifies the
// WebContentsDelegate::OpenURLFromTab implementation.
IN_PROC_BROWSER_TEST_F(IndigoOnboardingDialogBrowserTest, MiddleClickLink) {
  tabs::TabInterface* tab = browser()->GetActiveTabInterface();
  ASSERT_TRUE(tab);

  const GURL onboarding_url = embedded_test_server()->GetURL("/links.html");

  RunTestSequence(
      Do([&]() { OpenDialog(*tab, onboarding_url); }),
      WaitForShow(IndigoOnboardingDialog::kWebViewId),
      InstrumentNonTabWebView(kDialogWebContentsId,
                              IndigoOnboardingDialog::kWebViewId),
      WaitForWebContentsReady(kDialogWebContentsId, onboarding_url),
      InstrumentNextTab(kNewTabId),
      ClickElement(kDialogWebContentsId, DeepQuery{"#title1"},
                   ui_controls::MIDDLE)
          .SetMustRemainVisible(false),
      WaitForWebContentsReady(kNewTabId,
                              embedded_test_server()->GetURL("/title1.html")));
}

IN_PROC_BROWSER_TEST_F(IndigoOnboardingDialogBrowserTest,
                       SetResultBeforeClose) {
  tabs::TabInterface* tab = browser()->GetActiveTabInterface();
  ASSERT_TRUE(tab);

  const GURL onboarding_url = embedded_test_server()->GetURL("/empty.html");
  RunTestSequence(
      Do([&]() { OpenDialog(*tab, onboarding_url); }),
      WaitForShow(IndigoOnboardingDialog::kWebViewId),
      InstrumentNonTabWebView(kDialogWebContentsId,
                              IndigoOnboardingDialog::kWebViewId),
      WaitForWebContentsReady(kDialogWebContentsId, onboarding_url),
      CheckJsResult(kDialogWebContentsId,
                    "() => typeof window.chromeOnboarding", "object"),
      ExecuteJs(kDialogWebContentsId,
                R"js(
                  () => {
                    window.chromeOnboarding.acknowledgeChromeDisclaimer();
                    window.close();
                  }
                )js",
                ExecuteJsMode::kFireAndForget),
      WaitForHide(IndigoOnboardingDialog::kWebViewId),
      Check([&]() { return WasDialogClosed(); }),
      Check([&]() { return last_result_.acknowledge_chrome_disclaimer; }));
}

// Ensure that a dictionary parameter passed to this function is ignored.
// This just protects our ability to add more to this function in the future if
// needed, since this aspect of gin bindings isn't obvious.
IN_PROC_BROWSER_TEST_F(IndigoOnboardingDialogBrowserTest,
                       AcknowledgeWithParam) {
  GURL onboarding_url = embedded_test_server()->GetURL("/empty.html");
  tabs::TabInterface* tab = browser()->GetActiveTabInterface();

  RunTestSequence(Do([&]() { OpenDialog(*tab, onboarding_url); }),
                  WaitForShow(IndigoOnboardingDialog::kWebViewId),
                  InstrumentNonTabWebView(kDialogWebContentsId,
                                          IndigoOnboardingDialog::kWebViewId),
                  WaitForWebContentsReady(kDialogWebContentsId, onboarding_url),
                  ExecuteJs(kDialogWebContentsId,
                            R"js(
                  () => {
                    window.chromeOnboarding.acknowledgeChromeDisclaimer({});
                    window.close();
                  }
                )js",
                            ExecuteJsMode::kFireAndForget),
                  WaitForHide(IndigoOnboardingDialog::kWebViewId),
                  Check([&]() { return WasDialogClosed(); }), Check([&]() {
                    return last_result_.acknowledge_chrome_disclaimer;
                  }));
}

// Tests that acknowledging the disclaimer works after the onboarding dialog
// navigates to an isolated origin (i.e. a RenderFrameHost swap occurs during
// the initial navigation).
IN_PROC_BROWSER_TEST_F(IndigoOnboardingDialogBrowserTest, CrossRfhNavigation) {
  GURL onboarding_url =
      embedded_test_server()->GetURL("isolated.com", "/empty.html");
  tabs::TabInterface* tab = browser()->GetActiveTabInterface();

  RunTestSequence(
      Do([&]() { OpenDialog(*tab, onboarding_url); }),
      WaitForShow(IndigoOnboardingDialog::kWebViewId),
      InstrumentNonTabWebView(kDialogWebContentsId,
                              IndigoOnboardingDialog::kWebViewId),
      WaitForWebContentsReady(kDialogWebContentsId, onboarding_url),
      CheckJsResult(kDialogWebContentsId,
                    "() => typeof window.chromeOnboarding", "object"),
      ExecuteJs(kDialogWebContentsId,
                R"js(
                  () => {
                    window.chromeOnboarding.acknowledgeChromeDisclaimer();
                    window.close();
                  }
                )js",
                ExecuteJsMode::kFireAndForget),
      WaitForHide(IndigoOnboardingDialog::kWebViewId),
      Check([&]() { return WasDialogClosed(); }),
      Check([&]() { return last_result_.acknowledge_chrome_disclaimer; }));
}

IN_PROC_BROWSER_TEST_F(IndigoOnboardingDialogBrowserTest, CloseOnTabReload) {
  tabs::TabInterface* tab = browser()->GetActiveTabInterface();
  ASSERT_TRUE(tab);

  const GURL example_url("https://www.example.com/");
  RunTestSequence(Do([&]() { OpenDialog(*tab, example_url); }),
                  WaitForShow(IndigoOnboardingDialog::kWebViewId),
                  PressButton(kReloadButtonElementId),
                  WaitForHide(IndigoOnboardingDialog::kWebViewId),
                  Check([&]() { return WasDialogClosed(); }));
}

}  // namespace
}  // namespace indigo
