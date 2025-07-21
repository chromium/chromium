// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sessions/browser_list_router_helper.h"

#include <memory>
#include <vector>

#include "base/containers/contains.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/resource_coordinator/tab_manager.h"
#include "chrome/browser/sync/sessions/sync_sessions_web_contents_router.h"
#include "chrome/browser/sync/sessions/sync_sessions_web_contents_router_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/sync_sessions/synced_tab_delegate.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"

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
 protected:
  ~BrowserListRouterHelperBrowserTest() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    ProfileManager* profile_manager = g_browser_process->profile_manager();
    base::FilePath path_profile2 =
        profile_manager->user_data_dir().Append(FILE_PATH_LITERAL("profile_2"));

    base::RunLoop run_loop;
    profile_manager->CreateProfileAsync(
        path_profile2, base::BindLambdaForTesting([&](Profile* profile) {
          profile_2_ = profile;
          run_loop.Quit();
        }));
    run_loop.Run();
    ASSERT_NE(nullptr, profile_2_);
  }

  void TearDownOnMainThread() override {
    // `profile_2_` is owned by `ProfileManager`, and will be cleaned up
    // as part of `InProcessBrowserTest::TearDown`.
    profile_2_ = nullptr;
    InProcessBrowserTest::TearDownOnMainThread();
  }

#if BUILDFLAG(IS_CHROMEOS)
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(
        ash::switches::kIgnoreUserProfileMappingForTests);
  }
#endif

  MockLocalSessionEventHandler handler_1;
  MockLocalSessionEventHandler handler_2;
  raw_ptr<Profile> profile_2_ = nullptr;
};

IN_PROC_BROWSER_TEST_F(BrowserListRouterHelperBrowserTest,
                       ObservationScopedToSingleProfile) {
  Profile* profile_1 = GetProfile();
  Profile* profile_2 = profile_2_;

  Browser* browser_2 = CreateBrowser(profile_2);

  SyncSessionsWebContentsRouterFactory::GetInstance()
      ->GetForProfile(profile_1)
      ->StartRoutingTo(&handler_1);
  SyncSessionsWebContentsRouterFactory::GetInstance()
      ->GetForProfile(profile_2)
      ->StartRoutingTo(&handler_2);

  GURL gurl_1("https://foo1.com");
  GURL gurl_2("https://foo2.com");
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), gurl_1, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser_2, gurl_2, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  std::vector<GURL>* handler_1_urls = handler_1.seen_urls();
  EXPECT_TRUE(base::Contains(*handler_1_urls, gurl_1));
  EXPECT_FALSE(base::Contains(*handler_1_urls, gurl_2));

  std::vector<GURL>* handler_2_urls = handler_2.seen_urls();
  EXPECT_TRUE(base::Contains(*handler_2_urls, gurl_2));
  EXPECT_FALSE(base::Contains(*handler_2_urls, gurl_1));

  // Add a browser for each profile.
  Browser* new_browser_in_first_profile = CreateBrowser(profile_1);
  Browser* new_browser_in_second_profile = CreateBrowser(profile_2);

  GURL gurl_3("https://foo3.com");
  GURL gurl_4("https://foo4.com");
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      new_browser_in_first_profile, gurl_3,
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      new_browser_in_second_profile, gurl_4,
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  handler_1_urls = handler_1.seen_urls();
  EXPECT_TRUE(base::Contains(*handler_1_urls, gurl_3));
  EXPECT_FALSE(base::Contains(*handler_1_urls, gurl_4));

  handler_2_urls = handler_2.seen_urls();
  EXPECT_TRUE(base::Contains(*handler_2_urls, gurl_4));
  EXPECT_FALSE(base::Contains(*handler_2_urls, gurl_3));
}

// Added when fixing https://crbug.com/777745, ensure tab discards are observed.
IN_PROC_BROWSER_TEST_F(BrowserListRouterHelperBrowserTest, NotifyOnDiscardTab) {
  Profile* profile_1 = GetProfile();
  Profile* profile_2 = profile_2_;

  CreateBrowser(profile_2);

  SyncSessionsWebContentsRouterFactory::GetInstance()
      ->GetForProfile(profile_1)
      ->StartRoutingTo(&handler_1);
  SyncSessionsWebContentsRouterFactory::GetInstance()
      ->GetForProfile(profile_2)
      ->StartRoutingTo(&handler_2);

  GURL gurl_1("https://foo1.com");
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), gurl_1, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  // Tab needs to have been active to be found when discarding.
  BrowserList::GetInstance()->SetLastActive(browser());

  EXPECT_EQ(gurl_1, *handler_1.seen_urls()->rbegin());
  SessionID old_id = *handler_1.seen_ids()->rbegin();

  // Remove previous any observations from setup to make checking expectations
  // easier below.
  handler_1.seen_urls()->clear();
  handler_1.seen_ids()->clear();

  g_browser_process->GetTabManager()->DiscardTabByExtension(
      browser()->tab_strip_model()->GetActiveWebContents());

  // We're typically notified twice while discarding tabs. Once for the
  // destruction of the old web contents, and once for the new. This test case
  // is really trying to make sure the TabReplacedAt() method is called, which
  // is going to be invoked for the new web contents. We can tell it is the new
  // one by finding `gurl_1` for an id that is not `old_id`.
  bool found_new_id = false;
  for (size_t i = 0; i < handler_1.seen_ids()->size(); ++i) {
    if (handler_1.seen_ids()->at(i) != old_id &&
        handler_1.seen_urls()->at(i) == gurl_1) {
      found_new_id = true;
      break;
    }
  }

  // If WebContentsDiscard is enabled the tab and its associated WebContents are
  // not replaced during discard, so `profile_1` shouldn't have seen anything.
  // If disabled the tab's old WebContents is replaced with a new WebContents
  // and TabReplacedAt() should have been called.
  if (base::FeatureList::IsEnabled(features::kWebContentsDiscard)) {
    EXPECT_FALSE(found_new_id);
    EXPECT_EQ(0U, handler_1.seen_urls()->size());
  } else {
    EXPECT_TRUE(found_new_id);
    EXPECT_GT(0U, handler_1.seen_urls()->size());
  }

  // And of course `profile_2` shouldn't have seen anything.
  EXPECT_EQ(0U, handler_2.seen_urls()->size());
}

}  // namespace sync_sessions
