// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "base/strings/strcat.h"
#include "chrome/browser/search/ntp_features.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_notification_tracker.h"
#include "net/dns/mock_host_resolver.h"

// Verifies the Local NTP's navigation behavior. The Local NTP's site instance
// is chrome-search://local-ntp/local-ntp.html. See WebUiNtpBrowserTest for the
// WebUI NTP.
class LocalNtpNavigationBrowserTest : public InProcessBrowserTest {
 public:
  LocalNtpNavigationBrowserTest() {
    feature_list_.InitAndDisableFeature(ntp_features::kWebUI);
  }

  ~LocalNtpNavigationBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->StartAcceptingConnections();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Verify that the local NTP commits in a SiteInstance with the local NTP URL.
IN_PROC_BROWSER_TEST_F(LocalNtpNavigationBrowserTest, VerifySiteInstance) {
  GURL ntp_url(chrome::kChromeUINewTabURL);
  ui_test_utils::NavigateToURL(browser(), ntp_url);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_EQ(ntp_url, web_contents->GetLastCommittedURL());

  GURL local_ntp_url(base::StrCat({chrome::kChromeSearchScheme, "://",
                                   chrome::kChromeSearchLocalNtpHost, "/"}));

  ASSERT_EQ(local_ntp_url,
            web_contents->GetMainFrame()->GetSiteInstance()->GetSiteURL());
}

IN_PROC_BROWSER_TEST_F(LocalNtpNavigationBrowserTest, NtpProcesses) {
  // Listen for notifications about renderer processes being terminated - this
  // shouldn't happen during the test.
  content::TestNotificationTracker process_termination_tracker;
  process_termination_tracker.ListenFor(
      content::NOTIFICATION_RENDERER_PROCESS_TERMINATED,
      content::NotificationService::AllBrowserContextsAndSources());

  // Open a new tab and capture the initial state of the browser.
  chrome::NewTab(browser());
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(1, browser()->tab_strip_model()->active_index());
  content::WebContents* tab1 =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_NO_FATAL_FAILURE(EXPECT_TRUE(content::WaitForLoadStop(tab1)));
  int tab1_process_id = tab1->GetMainFrame()->GetProcess()->GetID();
  int initial_spare_process_id = -1;
  {
    content::RenderProcessHost* spare =
        content::RenderProcessHost::GetSpareRenderProcessHostForTesting();
    ASSERT_TRUE(spare);
    initial_spare_process_id = spare->GetID();
  }
  // Local NTP cannot reuse the spare process.
  EXPECT_NE(tab1_process_id, initial_spare_process_id);
  // No processes should be unnecessarily terminated.
  EXPECT_EQ(0u, process_termination_tracker.size());

  // Open another new tab and capture the resulting state of the browser.
  chrome::NewTab(browser());
  EXPECT_EQ(3, browser()->tab_strip_model()->count());
  EXPECT_EQ(2, browser()->tab_strip_model()->active_index());
  content::WebContents* tab2 =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_NO_FATAL_FAILURE(EXPECT_TRUE(content::WaitForLoadStop(tab2)));
  EXPECT_EQ(tab1->GetLastCommittedURL(), tab2->GetLastCommittedURL());
  EXPECT_EQ(tab1->GetVisibleURL(), tab2->GetVisibleURL());
  int tab2_process_id = tab2->GetMainFrame()->GetProcess()->GetID();
  int current_spare_process_id = -1;
  {
    content::RenderProcessHost* spare =
        content::RenderProcessHost::GetSpareRenderProcessHostForTesting();
    ASSERT_TRUE(spare);
    current_spare_process_id = spare->GetID();
  }
  EXPECT_NE(tab1_process_id, current_spare_process_id);
  EXPECT_NE(tab2_process_id, current_spare_process_id);

  // Verify that:
  // 1. Process-per-site is used for the Local NTP.  This just captures the
  //    current behavior without any value judgement.  Process-per-site
  //    translates into:
  //      1.1. |tab1| and |tab2| share the same process
  //      1.2. |tab2| does not use the spare process
  // 2. The initial spare process wasn't replaced with a new spare process
  //    The churn is undesirable since (per item 1.2. above) the initial spare
  //    is not used for |tab2|.  This is the main part of the verification and a
  //    regression test for https://crbug.com/1029345.
  EXPECT_EQ(tab1_process_id, tab2_process_id);                    // 1.1.
  EXPECT_NE(initial_spare_process_id, tab2_process_id);           // 1.2.
  EXPECT_EQ(initial_spare_process_id, current_spare_process_id);  // 2.

  // Verify that no processes were be unnecessarily terminated.
  EXPECT_EQ(0u, process_termination_tracker.size());
}
