// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/indigo/onboarding/indigo_onboarding_dialog.h"

#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/public/tab_dialog_manager.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace indigo {
namespace {

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
          gfx::Size(800, 600)),
      Do([&]() { dialog_->Close(); }),
      WaitForHide(IndigoOnboardingDialog::kWebViewId),
      Check([&]() { return WasDialogClosed(); }));
}

}  // namespace
}  // namespace indigo
