// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "base/version.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/test/base/in_process_browser_test.h"
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
  void SetUp() override {
    if (GetAshChromeVersion() < base::Version({118, 0, 5962})) {
      // For the older ash version without the ash browser window API
      // support in crosapi::mojom::TestController, we can't verify and close
      // feedback SWA in ash. Therefore, it still needs to run against the
      // unique ash.
      // TODO(crbug/1446083): Remove the unique ash code once ash stable
      // version >= 118.0.5962.0.
      StartUniqueAshChrome(
          {}, {}, {}, "crbug.com/1446083 The test leaves Ash windows behind");
    }
    InProcessBrowserTest::SetUp();
  }

  void ShowFeedbackPageWithFeedbackSource(chrome::FeedbackSource source) {
    std::string unused;
    chrome::ShowFeedbackPage(browser(), source,
                             /*description_template=*/unused,
                             /*description_placeholder_text=*/unused,
                             /*category_tag=*/unused,
                             /*extra_diagnostics=*/unused,
                             /*autofill_metadata=*/base::Value::Dict());
    if (IsCloseAndWaitAshBrowserWindowApisSupported()) {
      VerifyFeedbackPageShownInAsh();
    }
  }

 private:
  void VerifyFeedbackPageShownInAsh() {
    // There has not been a convenient way to verify a specific UI in Ash from
    // Lacros yet. Therefore, we just verify there is an Ash window opened
    // since Feedback UI is a SWA.
    WaitUntilAtLeastOneAshBrowserWindowOpen();
  }

  void TearDownOnMainThread() override {
    if (IsCloseAndWaitAshBrowserWindowApisSupported()) {
      CloseAllAshBrowserWindows();
    }
    InProcessBrowserTest::TearDownOnMainThread();
  }
};

IN_PROC_BROWSER_TEST_F(ShowFeedbackPageBrowserTest,
                       ShowFeedbackPageFromBrowserCommand) {
  base::HistogramTester histogram_tester;
  ShowFeedbackPageWithFeedbackSource(chrome::kFeedbackSourceBrowserCommand);
  histogram_tester.ExpectTotalCount("Feedback.RequestSource", 1);
}

IN_PROC_BROWSER_TEST_F(ShowFeedbackPageBrowserTest,
                       ShowFeedbackPageFromBrowserSettingsAboutPage) {
  base::HistogramTester histogram_tester;
  ShowFeedbackPageWithFeedbackSource(
      chrome::kFeedbackSourceMdSettingsAboutPage);
  histogram_tester.ExpectTotalCount("Feedback.RequestSource", 1);
}

IN_PROC_BROWSER_TEST_F(ShowFeedbackPageBrowserTest,
                       ShowFeedbackPageFromAutofillContextMenu) {
  base::HistogramTester histogram_tester;
  ShowFeedbackPageWithFeedbackSource(
      chrome::kFeedbackSourceAutofillContextMenu);
  histogram_tester.ExpectTotalCount("Feedback.RequestSource", 1);
}

IN_PROC_BROWSER_TEST_F(ShowFeedbackPageBrowserTest,
                       ShowFeedbackPageFromQuickAnswers) {
  base::HistogramTester histogram_tester;
  ShowFeedbackPageWithFeedbackSource(chrome::kFeedbackSourceQuickAnswers);
  histogram_tester.ExpectTotalCount("Feedback.RequestSource", 1);
}

IN_PROC_BROWSER_TEST_F(ShowFeedbackPageBrowserTest,
                       ShowFeedbackPageFromChromeLabs) {
  base::HistogramTester histogram_tester;
  ShowFeedbackPageWithFeedbackSource(chrome::kFeedbackSourceChromeLabs);
  histogram_tester.ExpectTotalCount("Feedback.RequestSource", 1);
}

IN_PROC_BROWSER_TEST_F(ShowFeedbackPageBrowserTest,
                       ShowFeedbackPageFromSadTabPage) {
  base::HistogramTester histogram_tester;
  ShowFeedbackPageWithFeedbackSource(chrome::kFeedbackSourceSadTabPage);
  histogram_tester.ExpectTotalCount("Feedback.RequestSource", 1);
}

IN_PROC_BROWSER_TEST_F(ShowFeedbackPageBrowserTest,
                       ShowFeedbackPageFromWindowLayoutMenu) {
  base::HistogramTester histogram_tester;
  ShowFeedbackPageWithFeedbackSource(chrome::kFeedbackSourceWindowLayoutMenu);
  histogram_tester.ExpectTotalCount("Feedback.RequestSource", 1);
}

IN_PROC_BROWSER_TEST_F(ShowFeedbackPageBrowserTest,
                       ShowFeedbackPageFromCookieControls) {
  base::HistogramTester histogram_tester;
  ShowFeedbackPageWithFeedbackSource(chrome::kFeedbackSourceCookieControls);
  histogram_tester.ExpectTotalCount("Feedback.RequestSource", 1);
}

IN_PROC_BROWSER_TEST_F(ShowFeedbackPageBrowserTest,
                       ShowFeedbackPageFromSettingsPerformancePage) {
  base::HistogramTester histogram_tester;
  ShowFeedbackPageWithFeedbackSource(
      chrome::kFeedbackSourceSettingsPerformancePage);
  histogram_tester.ExpectTotalCount("Feedback.RequestSource", 1);
}

IN_PROC_BROWSER_TEST_F(ShowFeedbackPageBrowserTest,
                       ShowFeedbackPageFromProfileErrorDialog) {
  base::HistogramTester histogram_tester;
  ShowFeedbackPageWithFeedbackSource(chrome::kFeedbackSourceProfileErrorDialog);
  histogram_tester.ExpectTotalCount("Feedback.RequestSource", 1);
}

IN_PROC_BROWSER_TEST_F(ShowFeedbackPageBrowserTest,
                       ShowFeedbackPageFromQuickOffice) {
  base::HistogramTester histogram_tester;
  ShowFeedbackPageWithFeedbackSource(chrome::kFeedbackSourceQuickOffice);
  histogram_tester.ExpectTotalCount("Feedback.RequestSource", 1);
}
