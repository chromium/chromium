// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/indigo/onboarding/indigo_onboarding_dialog.h"

#include "base/functional/callback_helpers.h"
#include "base/strings/escape.h"
#include "base/test/bind.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/public/tab_dialog_manager.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/tabs/public/tab_interface.h"
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

GURL HTMLToDataView(std::string_view html) {
  return GURL("data:text/html;charset=utf-8," +
              base::EscapeAllExceptUnreserved(html));
}

class IndigoOnboardingDialogBrowserTest : public InteractiveBrowserTest {
 protected:
  void OpenDialog(tabs::TabInterface& tab, const GURL& url) {
    dialog_ = IndigoOnboardingDialog::Show(tab, url,
                                           base::BindLambdaForTesting([&]() {
                                             closed_ = true;
                                             dialog_.reset();
                                           }));
  }

  bool WasDialogClosed() const { return closed_; }

  std::unique_ptr<IndigoOnboardingDialog> dialog_;
  bool closed_ = false;
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
          SizeIsInRange(gfx::Size(480, 360), gfx::Size(480, 600))),
      Do([&]() { dialog_->Close(); }),
      WaitForHide(IndigoOnboardingDialog::kWebViewId),
      Check([&]() { return WasDialogClosed(); }));
}


IN_PROC_BROWSER_TEST_F(IndigoOnboardingDialogBrowserTest, JSWindowClose) {
  tabs::TabInterface* tab = browser()->GetActiveTabInterface();
  ASSERT_TRUE(tab);

  const GURL onboarding_url("about:blank");
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

  const GURL onboarding_url = HTMLToDataView(R"html(
    <!DOCTYPE html>
    <html><body>
      <a id="link-blank" href="about:blank" target="_blank">Blank</a>
    </body></html>
  )html");

  RunTestSequence(Do([&]() { OpenDialog(*tab, onboarding_url); }),
                  WaitForShow(IndigoOnboardingDialog::kWebViewId),
                  InstrumentNonTabWebView(kDialogWebContentsId,
                                          IndigoOnboardingDialog::kWebViewId),
                  WaitForWebContentsReady(kDialogWebContentsId, onboarding_url),
                  InstrumentNextTab(kNewTabId),
                  ClickElement(kDialogWebContentsId, DeepQuery{"#link-blank"})
                      .SetMustRemainVisible(false),
                  WaitForWebContentsReady(kNewTabId, GURL("about:blank")));
}

// Test middle-clicking a link. This verifies the
// WebContentsDelegate::OpenURLFromTab implementation.
IN_PROC_BROWSER_TEST_F(IndigoOnboardingDialogBrowserTest, MiddleClickLink) {
  tabs::TabInterface* tab = browser()->GetActiveTabInterface();
  ASSERT_TRUE(tab);

  const GURL onboarding_url = HTMLToDataView(R"html(
    <!DOCTYPE html>
    <html><body>
      <a id="link-normal" href="about:blank">Normal</a>
    </body></html>
  )html");

  RunTestSequence(Do([&]() { OpenDialog(*tab, onboarding_url); }),
                  WaitForShow(IndigoOnboardingDialog::kWebViewId),
                  InstrumentNonTabWebView(kDialogWebContentsId,
                                          IndigoOnboardingDialog::kWebViewId),
                  WaitForWebContentsReady(kDialogWebContentsId, onboarding_url),
                  InstrumentNextTab(kNewTabId),
                  ClickElement(kDialogWebContentsId, DeepQuery{"#link-normal"},
                               ui_controls::MIDDLE)
                      .SetMustRemainVisible(false),
                  WaitForWebContentsReady(kNewTabId, GURL("about:blank")));
}

}  // namespace
}  // namespace indigo
