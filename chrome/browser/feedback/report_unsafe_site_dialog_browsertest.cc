// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feedback/report_unsafe_site_dialog.h"

#include <string>

#include "base/metrics/statistics_recorder.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/platform_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/widget/widget.h"

// Browser tests for report-unsafe-site dialog.
class ReportUnsafeSiteDialogBrowserTest : public PlatformBrowserTest {
 public:
  ReportUnsafeSiteDialogBrowserTest() = default;
  ~ReportUnsafeSiteDialogBrowserTest() override = default;

  content::WebContents* web_contents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  void NavigateAndCheckTitle(const GURL& url,
                             const std::u16string& expected_title) {
    content::TitleWatcher title_watcher(web_contents(), expected_title);
    ASSERT_TRUE(content::NavigateToURL(web_contents(), url));
    EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
  }

  content::WebContents* OpenDialogAndGetWebContents() {
    content::WebContentsAddedObserver web_contents_added_observer;
    feedback::ReportUnsafeSiteDialog::Show(browser());
    content::WebContents* dialog_contents =
        web_contents_added_observer.GetWebContents();
    content::WaitForLoadStop(dialog_contents);
    return dialog_contents;
  }

  void ClickInDialog(content::WebContents* web_contents,
                     const std::string& css_selector) {
    content::ExecuteScriptAsync(
        web_contents,
        base::StringPrintf(
            "document.querySelector('report-unsafe-site-app').shadowRoot."
            "querySelector('%s').click()",
            css_selector.c_str()));
  }

  void WaitUntilButtonEnabled(content::WebContents* web_contents,
                              const std::string& css_selector) {
    const std::string script = base::StringPrintf(
        "new Promise(resolve => {"
        "  const btn = document.querySelector('report-unsafe-site-app')"
        "      .shadowRoot.querySelector('%s');"
        "  if (!btn.disabled) {"
        "    resolve(true);"
        "  } else {"
        "    const obs = new MutationObserver(() => {"
        "      if (!btn.disabled) {"
        "        obs.disconnect();"
        "        resolve(true);"
        "      }"
        "    });"
        "    obs.observe(btn, { attributes: true });"
        "  }"
        "});",
        css_selector.c_str());
    EXPECT_TRUE(content::EvalJs(web_contents, script).ExtractBool());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kReportUnsafeSite};
};

// Test that chrome://feedback/report-unsafe-site cannot be navigated to when it
// is disabled by enterprise policy.
IN_PROC_BROWSER_TEST_F(ReportUnsafeSiteDialogBrowserTest,
                       DisabledByPolicy_TryNavigate) {
  PrefService* prefs = browser()->profile()->GetPrefs();
  prefs->SetBoolean(prefs::kSafeBrowsingEnabled, true);
  prefs->SetBoolean(prefs::kUserFeedbackAllowed, false);

  ASSERT_TRUE(content::NavigateToURL(web_contents(), GURL("about:blank")));
  ASSERT_FALSE(content::NavigateToURL(
      web_contents(), GURL("chrome://feedback/report-unsafe-site")));
  EXPECT_EQ(
      content::PageType::PAGE_TYPE_ERROR,
      web_contents()->GetController().GetLastCommittedEntry()->GetPageType());

  browser()->profile()->GetPrefs()->SetBoolean(prefs::kUserFeedbackAllowed,
                                               true);
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(), GURL("chrome://feedback/report-unsafe-site")));
}

// Test that chrome://feedback/report-unsafe-site redirects to chrome://feedback
IN_PROC_BROWSER_TEST_F(ReportUnsafeSiteDialogBrowserTest,
                       DisabledBySafeBrowsingPrefs_TryNavigate) {
  std::u16string feedback_title =
      l10n_util::GetStringUTF16(IDS_FEEDBACK_REPORT_APP_TITLE);
  std::u16string report_unsafe_site_title =
      l10n_util::GetStringUTF16(IDS_REPORT_UNSAFE_SITE_DIALOG_TITLE);
  ASSERT_NE(feedback_title, report_unsafe_site_title);

  // TODO(crbug.com/478306738): Add test for when Safe Browsing is disabled.

  PrefService* prefs = browser()->profile()->GetPrefs();
  prefs->SetBoolean(prefs::kSafeBrowsingEnabled, true);
  ASSERT_TRUE(content::NavigateToURL(web_contents(), GURL("about:blank")));
  NavigateAndCheckTitle(GURL("chrome://feedback/report-unsafe-site"),
                        report_unsafe_site_title);
}

IN_PROC_BROWSER_TEST_F(ReportUnsafeSiteDialogBrowserTest,
                       RecordsIsTabSplitHistogram_NotSplit) {
  base::HistogramTester histogram_tester;
  feedback::ReportUnsafeSiteDialog::Show(browser());
  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.ReportUnsafeSiteDialog.IsTabSplit", false, 1);
}

IN_PROC_BROWSER_TEST_F(ReportUnsafeSiteDialogBrowserTest,
                       RecordsIsTabSplitHistogram_Split) {
  base::HistogramTester histogram_tester;

  chrome::NewTab(browser());
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  EXPECT_EQ(tab_strip_model->count(), 2);

  // Split the current tab.
  tab_strip_model->ExecuteContextMenuCommand(1,
                                             TabStripModel::CommandAddToSplit);

  feedback::ReportUnsafeSiteDialog::Show(browser());
  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.ReportUnsafeSiteDialog.IsTabSplit", true, 1);
}

IN_PROC_BROWSER_TEST_F(ReportUnsafeSiteDialogBrowserTest, CloseReason_Cancel) {
  constexpr char kHistogramName[] =
      "SafeBrowsing.ReportUnsafeSite.DialogClosedReason";
  base::HistogramTester histogram_tester;
  base::StatisticsRecorder::HistogramWaiter histogram_waiter(kHistogramName);

  content::WebContents* dialog_contents = OpenDialogAndGetWebContents();
  ClickInDialog(dialog_contents, "#cancel-button");

  histogram_waiter.Wait();
  histogram_tester.ExpectUniqueSample(
      kHistogramName, views::Widget::ClosedReason::kCancelButtonClicked, 1);
}

IN_PROC_BROWSER_TEST_F(ReportUnsafeSiteDialogBrowserTest, CloseReason_Send) {
  constexpr char kHistogramName[] =
      "SafeBrowsing.ReportUnsafeSite.DialogClosedReason";
  base::HistogramTester histogram_tester;
  base::StatisticsRecorder::HistogramWaiter histogram_waiter(kHistogramName);

  content::WebContents* dialog_contents = OpenDialogAndGetWebContents();
  WaitUntilButtonEnabled(dialog_contents, ".action-button");
  ClickInDialog(dialog_contents, ".action-button");

  histogram_waiter.Wait();
  histogram_tester.ExpectUniqueSample(
      kHistogramName, views::Widget::ClosedReason::kAcceptButtonClicked, 1);
}

IN_PROC_BROWSER_TEST_F(ReportUnsafeSiteDialogBrowserTest, CloseReason_Escape) {
  constexpr char kHistogramName[] =
      "SafeBrowsing.ReportUnsafeSite.DialogClosedReason";
  base::HistogramTester histogram_tester;
  base::StatisticsRecorder::HistogramWaiter histogram_waiter(kHistogramName);

  content::WebContents* dialog_contents = OpenDialogAndGetWebContents();
  views::Widget* widget = views::Widget::GetWidgetForNativeWindow(
      dialog_contents->GetTopLevelNativeWindow());
  ui::Accelerator esc(ui::VKEY_ESCAPE, 0);
  EXPECT_TRUE(widget->client_view()->AcceleratorPressed(esc));

  histogram_waiter.Wait();
  histogram_tester.ExpectUniqueSample(
      kHistogramName, views::Widget::ClosedReason::kEscKeyPressed, 1);
}
