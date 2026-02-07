// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/platform_browser_test.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

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
