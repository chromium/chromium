// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feedback/feedback_disabled_dialog.h"

#include <string>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "ui/base/base_window.h"

class FeedbackDisabledDialogBrowserTest : public DialogBrowserTest {
 public:
  FeedbackDisabledDialogBrowserTest() = default;
  FeedbackDisabledDialogBrowserTest(const FeedbackDisabledDialogBrowserTest&) =
      delete;
  FeedbackDisabledDialogBrowserTest& operator=(
      const FeedbackDisabledDialogBrowserTest&) = delete;

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    if (name == "Enterprise") {
      browser()->GetProfile()->GetPrefs()->SetBoolean(
          prefs::kUserFeedbackAllowed, false);
    }
    chrome::ShowFeedbackDisabledDialog(
        browser()->GetWindow()->GetNativeWindow(), browser()->GetProfile());
  }
};

IN_PROC_BROWSER_TEST_F(FeedbackDisabledDialogBrowserTest, InvokeUi_Default) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(FeedbackDisabledDialogBrowserTest, InvokeUi_Enterprise) {
  ShowAndVerifyUi();
}
