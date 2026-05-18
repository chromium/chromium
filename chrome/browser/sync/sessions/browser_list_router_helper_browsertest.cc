// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sessions/browser_list_router_helper.h"

#include <algorithm>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/resource_coordinator/tab_manager.h"
#include "chrome/browser/sync/sessions/sync_sessions_web_contents_router.h"
#include "chrome/browser/sync/sessions/sync_sessions_web_contents_router_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/sync/browser_synced_tab_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/sync_sessions/synced_tab_delegate.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/constants/ash_switches.h"
#endif

namespace sync_sessions {

class MockLocalSessionEventHandler : public LocalSessionEventHandler {
 public:
  void OnSessionRestoreComplete() override {}

  void OnLocalTabModified(SyncedTabDelegate* modified_tab) override {
    seen_urls_.push_back(modified_tab->GetVirtualURLAtIndex(
        modified_tab->GetCurrentEntryIndex()));
    seen_ids_.push_back(modified_tab->GetSessionId());
  }

  void OnLocalTabClosed() override {}

  std::vector<GURL>* seen_urls() { return &seen_urls_; }
  std::vector<SessionID>* seen_ids() { return &seen_ids_; }

 private:
  std::vector<GURL> seen_urls_;
  std::vector<SessionID> seen_ids_;
};

class BrowserListRouterHelperBrowserTest : public InProcessBrowserTest {
 public:
  BrowserListRouterHelperBrowserTest() {
    scoped_feature_list_.InitAndDisableFeature(features::kWebContentsDiscard);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
#if BUILDFLAG(IS_CHROMEOS)
    command_line->AppendSwitch(
        ash::switches::kIgnoreUserProfileMappingForTests);
#endif
  }

  MockLocalSessionEventHandler& handler_1() { return handler_1_; }
  MockLocalSessionEventHandler& handler_2() { return handler_2_; }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  MockLocalSessionEventHandler handler_1_;
  MockLocalSessionEventHandler handler_2_;
};

IN_PROC_BROWSER_TEST_F(BrowserListRouterHelperBrowserTest,
                       ObservationScopedToSingleProfile) {
  ASSERT_TRUE(embedded_test_server()->Start());
  Profile* profile_1 = browser()->profile();

  // Create a second profile and browser.
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  Profile* profile_2 = &profiles::testing::CreateProfileSync(
      profile_manager, profile_manager->GenerateNextProfileDirectoryPath());
  ASSERT_TRUE(profile_2);

  Browser* browser_2 = CreateBrowser(profile_2);

  SyncSessionsWebContentsRouterFactory::GetInstance()
      ->GetForProfile(profile_1)
      ->StartRoutingTo(&handler_1());
  SyncSessionsWebContentsRouterFactory::GetInstance()
      ->GetForProfile(profile_2)
      ->StartRoutingTo(&handler_2());

  GURL gurl_1 = embedded_test_server()->GetURL("/title1.html");
  GURL gurl_2 = embedded_test_server()->GetURL("/title2.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), gurl_1));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser_2, gurl_2));

  std::vector<GURL>* handler_1_urls = handler_1().seen_urls();
  EXPECT_TRUE(std::ranges::contains(*handler_1_urls, gurl_1));
  EXPECT_FALSE(std::ranges::contains(*handler_1_urls, gurl_2));

  std::vector<GURL>* handler_2_urls = handler_2().seen_urls();
  EXPECT_TRUE(std::ranges::contains(*handler_2_urls, gurl_2));
  EXPECT_FALSE(std::ranges::contains(*handler_2_urls, gurl_1));

  // Add a browser for each profile.
  Browser* new_browser_in_first_profile = CreateBrowser(profile_1);
  Browser* new_browser_in_second_profile = CreateBrowser(profile_2);

  GURL gurl_3 = embedded_test_server()->GetURL("/title3.html");
  GURL gurl_4 = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(new_browser_in_first_profile, gurl_3));
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(new_browser_in_second_profile, gurl_4));

  handler_1_urls = handler_1().seen_urls();
  EXPECT_TRUE(std::ranges::contains(*handler_1_urls, gurl_3));
  EXPECT_FALSE(std::ranges::contains(*handler_1_urls, gurl_4));

  handler_2_urls = handler_2().seen_urls();
  EXPECT_TRUE(std::ranges::contains(*handler_2_urls, gurl_4));
  EXPECT_FALSE(std::ranges::contains(*handler_2_urls, gurl_3));
}

// Added when fixing https://crbug.com/40546261, ensure tab discards are
// observed.
IN_PROC_BROWSER_TEST_F(BrowserListRouterHelperBrowserTest, NotifyOnDiscardTab) {
  ASSERT_TRUE(embedded_test_server()->Start());
  Profile* profile_1 = browser()->profile();

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  Profile* profile_2 = &profiles::testing::CreateProfileSync(
      profile_manager, profile_manager->GenerateNextProfileDirectoryPath());
  ASSERT_TRUE(profile_2);

  // Create `browser_2` to set up `profile_2`'s environment so it is
  // possible to verify that the second profile does not observe any tab
  // discard events from `profile_1`.
  CreateBrowser(profile_2);

  SyncSessionsWebContentsRouterFactory::GetInstance()
      ->GetForProfile(profile_1)
      ->StartRoutingTo(&handler_1());
  SyncSessionsWebContentsRouterFactory::GetInstance()
      ->GetForProfile(profile_2)
      ->StartRoutingTo(&handler_2());

  GURL gurl_1 = embedded_test_server()->GetURL("/title1.html");
  GURL gurl_2 = embedded_test_server()->GetURL("/title2.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), gurl_1));
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), gurl_2, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  // Find old_id of gurl_1.
  SessionID old_id = SessionID::InvalidValue();
  for (size_t i = 0; i < handler_1().seen_urls()->size(); ++i) {
    if (handler_1().seen_urls()->at(i) == gurl_1) {
      old_id = handler_1().seen_ids()->at(i);
    }
  }
  ASSERT_TRUE(old_id.is_valid());

  // Remove previous any observations from setup to make checking expectations
  // easier below.
  handler_1().seen_urls()->clear();
  handler_1().seen_ids()->clear();

  content::WebContents* old_contents =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  content::WebContents* discarded =
      g_browser_process->GetTabManager()->DiscardTabByExtension(old_contents);
  ASSERT_TRUE(discarded);
  EXPECT_NE(old_contents, discarded);

  // The replacement and new SessionID generation occur synchronously during
  // DiscardTabByExtension. We don't need to reload or NavigateToURL here.

  // We're typically notified twice while discarding tabs. Once for the
  // destruction of the old web contents, and once for the new. This test case
  // is really trying to make sure the TabReplacedAt() method is called, which
  // is going to be invoked for the new web contents. We can tell it is the new
  // one by finding |gurl_1| for an id that is not |old_id|.
  bool found_new_id = false;
  for (size_t i = 0; i < handler_1().seen_ids()->size(); ++i) {
    if (handler_1().seen_ids()->at(i) != old_id &&
        handler_1().seen_urls()->at(i) == gurl_1) {
      found_new_id = true;
      break;
    }
  }
  EXPECT_TRUE(found_new_id);
  // And of course |profile_2| shouldn't have seen anything.
  EXPECT_EQ(0U, handler_2().seen_urls()->size());
}

}  // namespace sync_sessions
