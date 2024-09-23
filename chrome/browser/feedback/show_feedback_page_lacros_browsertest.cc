// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/contains.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/version.h"
#include "chrome/browser/feedback/show_feedback_page.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/startup/browser_params_proxy.h"
#include "components/version_info/version_info.h"
#include "content/public/test/browser_test.h"

// Tests invoking feedback report from Lacros with different feedback source.
// When adding a new value in LacrosFeedbackSource in
// chromeos/crosapi/mojom/feedback.mojom, a new test case should be added
// accordingly.
class ShowFeedbackPageBrowserTest : public InProcessBrowserTest {
 public:
  ShowFeedbackPageBrowserTest() = default;
  ~ShowFeedbackPageBrowserTest() override = default;

 protected:
  void ShowFeedbackPageWithFeedbackSource(feedback::FeedbackSource source) {
    std::string unused;
    chrome::ShowFeedbackPage(browser(), source,
                             /*description_template=*/unused,
                             /*description_placeholder_text=*/unused,
                             /*category_tag=*/unused,
                             /*extra_diagnostics=*/unused,
                             /*autofill_metadata=*/base::Value::Dict());
    VerifyFeedbackPageShownInAsh();
  }

 private:
  void VerifyFeedbackPageShownInAsh() {
    // There has not been a convenient way to verify a specific UI in Ash from
    // Lacros yet. Therefore, we just verify there is an Ash window opened
    // since Feedback UI is a SWA.
    WaitUntilAtLeastOneAshBrowserWindowOpen();
  }

  void TearDownOnMainThread() override {
    CloseAllAshBrowserWindows();

    InProcessBrowserTest::TearDownOnMainThread();
  }
};

IN_PROC_BROWSER_TEST_F(ShowFeedbackPageBrowserTest,
                       ShowFeedbackPageFromBrowserCommand) {
  base::HistogramTester histogram_tester;
  ShowFeedbackPageWithFeedbackSource(feedback::kFeedbackSourceBrowserCommand);
  histogram_tester.ExpectTotalCount("Feedback.RequestSource", 1);
}

IN_PROC_BROWSER_TEST_F(ShowFeedbackPageBrowserTest,
                       ShowFeedbackPageFromBrowserSettingsAboutPage) {
  base::HistogramTester histogram_tester;
  ShowFeedbackPageWithFeedbackSource(
      feedback::kFeedbackSourceMdSettingsAboutPage);
  histogram_tester.ExpectTotalCount("Feedback.RequestSource", 1);
}

IN_PROC_BROWSER_TEST_F(ShowFeedbackPageBrowserTest,
                       ShowFeedbackPageFromAutofillContextMenu) {
  base::HistogramTester histogram_tester;
  ShowFeedbackPageWithFeedbackSource(
      feedback::kFeedbackSourceAutofillContextMenu);
  histogram_tester.ExpectTotalCount("Feedback.RequestSource", 1);
}

IN_PROC_BROWSER_TEST_F(ShowFeedbackPageBrowserTest,
                       ShowFeedbackPageFromQuickAnswers) {
  base::HistogramTester histogram_tester;
  ShowFeedbackPageWithFeedbackSource(feedback::kFeedbackSourceQuickAnswers);
  histogram_tester.ExpectTotalCount("Feedback.RequestSource", 1);
}

IN_PROC_BROWSER_TEST_F(ShowFeedbackPageBrowserTest,
                       ShowFeedbackPageFromChromeLabs) {
  base::HistogramTester histogram_tester;
  ShowFeedbackPageWithFeedbackSource(feedback::kFeedbackSourceChromeLabs);
  histogram_tester.ExpectTotalCount("Feedback.RequestSource", 1);
}

IN_PROC_BROWSER_TEST_F(ShowFeedbackPageBrowserTest,
                       ShowFeedbackPageFromSadTabPage) {
  base::HistogramTester histogram_tester;
  ShowFeedbackPageWithFeedbackSource(feedback::kFeedbackSourceSadTabPage);
  histogram_tester.ExpectTotalCount("Feedback.RequestSource", 1);
}

IN_PROC_BROWSER_TEST_F(ShowFeedbackPageBrowserTest,
                       ShowFeedbackPageFromWindowLayoutMenu) {
  base::HistogramTester histogram_tester;
  ShowFeedbackPageWithFeedbackSource(feedback::kFeedbackSourceWindowLayoutMenu);
  histogram_tester.ExpectTotalCount("Feedback.RequestSource", 1);
}

IN_PROC_BROWSER_TEST_F(ShowFeedbackPageBrowserTest,
                       ShowFeedbackPageFromCookieControls) {
  base::HistogramTester histogram_tester;
  ShowFeedbackPageWithFeedbackSource(feedback::kFeedbackSourceCookieControls);
  histogram_tester.ExpectTotalCount("Feedback.RequestSource", 1);
}

IN_PROC_BROWSER_TEST_F(ShowFeedbackPageBrowserTest,
                       ShowFeedbackPageFromSettingsPerformancePage) {
  base::HistogramTester histogram_tester;
  ShowFeedbackPageWithFeedbackSource(
      feedback::kFeedbackSourceSettingsPerformancePage);
  histogram_tester.ExpectTotalCount("Feedback.RequestSource", 1);
}

IN_PROC_BROWSER_TEST_F(ShowFeedbackPageBrowserTest,
                       ShowFeedbackPageFromProfileErrorDialog) {
  base::HistogramTester histogram_tester;
  ShowFeedbackPageWithFeedbackSource(
      feedback::kFeedbackSourceProfileErrorDialog);
  histogram_tester.ExpectTotalCount("Feedback.RequestSource", 1);
}

IN_PROC_BROWSER_TEST_F(ShowFeedbackPageBrowserTest,
                       ShowFeedbackPageFromQuickOffice) {
  base::HistogramTester histogram_tester;
  ShowFeedbackPageWithFeedbackSource(feedback::kFeedbackSourceQuickOffice);
  histogram_tester.ExpectTotalCount("Feedback.RequestSource", 1);
}

IN_PROC_BROWSER_TEST_F(ShowFeedbackPageBrowserTest, ShowFeedbackPageFromAI) {
  base::HistogramTester histogram_tester;
  std::string unused;
  auto capabilities = chromeos::BrowserParamsProxy::Get()->AshCapabilities();
  if (!capabilities || !base::Contains(*capabilities, "crbug/1501057")) {
    GTEST_SKIP() << "Unsupported feedback source AI for ash.";
  }

  // AI flow uses the Chrome feedback dialog instead so no new ash window will
  // be created.
  chrome::ShowFeedbackPage(browser(), feedback::kFeedbackSourceAI,
                           /*description_template=*/unused,
                           /*description_placeholder_text=*/unused,
                           /*category_tag=*/unused,
                           /*extra_diagnostics=*/unused,
                           /*autofill_metadata=*/base::Value::Dict(),
                           /*ai_metadata=*/base::Value::Dict());
  histogram_tester.ExpectTotalCount("Feedback.RequestSource", 1);
}

IN_PROC_BROWSER_TEST_F(ShowFeedbackPageBrowserTest,
                       ShowFeedbackPageFromLensOverlay) {
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount("Feedback.RequestSource", 0);
  ShowFeedbackPageWithFeedbackSource(feedback::kFeedbackSourceLensOverlay);
  histogram_tester.ExpectTotalCount("Feedback.RequestSource", 1);
}
