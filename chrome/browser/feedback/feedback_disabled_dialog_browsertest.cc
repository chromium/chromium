// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feedback/feedback_disabled_dialog.h"

#include <string>

#include "base/files/file_path.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/feedback/show_feedback_page.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "ui/base/base_window.h"
#include "url/gurl.h"

class FeedbackDisabledDialogBrowserTest : public DialogBrowserTest {
 public:
  FeedbackDisabledDialogBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kFeedbackDisabledDialog);
  }
  FeedbackDisabledDialogBrowserTest(const FeedbackDisabledDialogBrowserTest&) =
      delete;
  FeedbackDisabledDialogBrowserTest& operator=(
      const FeedbackDisabledDialogBrowserTest&) = delete;

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    if (name == "ShowFeedbackPage") {
      browser()->GetProfile()->GetPrefs()->SetBoolean(
          prefs::kUserFeedbackAllowed, false);
      chrome::ShowFeedbackPage(browser(),
                               feedback::kFeedbackSourceBrowserCommand,
                               /*description_template=*/"",
                               /*description_placeholder_text=*/"",
                               /*category_tag=*/"",
                               /*extra_diagnostics=*/"");
      return;
    }

    if (name == "ShowFeedbackPage_NoParent") {
      browser()->GetProfile()->GetPrefs()->SetBoolean(
          prefs::kUserFeedbackAllowed, false);
      chrome::ShowFeedbackPage(GURL("about:blank"), browser()->GetProfile(),
                               feedback::kFeedbackSourceBrowserCommand,
                               /*description_template=*/"",
                               /*description_placeholder_text=*/"",
                               /*category_tag=*/"",
                               /*extra_diagnostics=*/"");
      return;
    }

    if (name == "Enterprise") {
      browser()->GetProfile()->GetPrefs()->SetBoolean(
          prefs::kUserFeedbackAllowed, false);
    }
    chrome::ShowFeedbackDisabledDialog(
        browser()->GetWindow()->GetNativeWindow(), browser()->GetProfile());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(FeedbackDisabledDialogBrowserTest, InvokeUi_Default) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(FeedbackDisabledDialogBrowserTest, InvokeUi_Enterprise) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(FeedbackDisabledDialogBrowserTest,
                       InvokeUi_ShowFeedbackPage) {
  base::HistogramTester histogram_tester;
  ShowAndVerifyUi();
  histogram_tester.ExpectUniqueSample(
      "Feedback.DisabledDialog.ParentStatus",
      chrome::FeedbackDisabledDialogParentStatus::kDirectParent, 1);
}

IN_PROC_BROWSER_TEST_F(FeedbackDisabledDialogBrowserTest,
                       InvokeUi_ShowFeedbackPage_NoParent) {
  base::HistogramTester histogram_tester;
  ShowAndVerifyUi();
  histogram_tester.ExpectUniqueSample(
      "Feedback.DisabledDialog.ParentStatus",
      chrome::FeedbackDisabledDialogParentStatus::kFoundByFallback, 1);
}

IN_PROC_BROWSER_TEST_F(FeedbackDisabledDialogBrowserTest,
                       ShowFeedbackPage_NoParent_NoBrowserWindow) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  base::FilePath path = profile_manager->GenerateNextProfileDirectoryPath();
  Profile& other_profile =
      profiles::testing::CreateProfileSync(profile_manager, path);
  other_profile.GetPrefs()->SetBoolean(prefs::kUserFeedbackAllowed, false);

  base::HistogramTester histogram_tester;
  chrome::ShowFeedbackPage(GURL("about:blank"), &other_profile,
                           feedback::kFeedbackSourceBrowserCommand,
                           /*description_template=*/"",
                           /*description_placeholder_text=*/"",
                           /*category_tag=*/"",
                           /*extra_diagnostics=*/"");
  histogram_tester.ExpectUniqueSample(
      "Feedback.DisabledDialog.ParentStatus",
      chrome::FeedbackDisabledDialogParentStatus::kNotFound, 1);
}
