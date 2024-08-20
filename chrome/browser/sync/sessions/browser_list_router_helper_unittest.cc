// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sessions/browser_list_router_helper.h"

#include <memory>
#include <vector>

#include "base/containers/contains.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/resource_coordinator/tab_manager.h"
#include "chrome/browser/sync/sessions/sync_sessions_web_contents_router.h"
#include "chrome/browser/sync/sessions/sync_sessions_web_contents_router_factory.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/sync_sessions/synced_tab_delegate.h"

namespace sync_sessions {

class MockLocalSessionEventHandler : public LocalSessionEventHandler {
 public:
  void OnSessionRestoreComplete() override {}

  void OnLocalTabModified(SyncedTabDelegate* modified_tab) override {
    seen_urls_.push_back(modified_tab->GetVirtualURLAtIndex(
        modified_tab->GetCurrentEntryIndex()));
    seen_ids_.push_back(modified_tab->GetSessionId());
  }

  std::vector<GURL>* seen_urls() { return &seen_urls_; }
  std::vector<SessionID>* seen_ids() { return &seen_ids_; }

 private:
  std::vector<GURL> seen_urls_;
  std::vector<SessionID> seen_ids_;
};

class BrowserListRouterHelperTest : public BrowserWithTestWindowTest {
 protected:
  ~BrowserListRouterHelperTest() override = default;

  MockLocalSessionEventHandler handler_1;
  MockLocalSessionEventHandler handler_2;
};

TEST_F(BrowserListRouterHelperTest, ObservationScopedToSingleProfile) {
  TestingProfile* profile_1 = profile();
  TestingProfile* profile_2 =
      profile_manager()->CreateTestingProfile("testing_profile2");

  std::unique_ptr<BrowserWindow> window_2(CreateBrowserWindow());
  std::unique_ptr<Browser> browser_2(
      CreateBrowser(profile_2, browser()->type(), false, window_2.get()));

  SyncSessionsWebContentsRouterFactory::GetInstance()
      ->GetForProfile(profile_1)
      ->StartRoutingTo(&handler_1);
  SyncSessionsWebContentsRouterFactory::GetInstance()
      ->GetForProfile(profile_2)
      ->StartRoutingTo(&handler_2);

  GURL gurl_1("http://foo1.com");
  GURL gurl_2("http://foo2.com");
  AddTab(browser(), gurl_1);
  AddTab(browser_2.get(), gurl_2);

  std::vector<GURL>* handler_1_urls = handler_1.seen_urls();
  EXPECT_TRUE(base::Contains(*handler_1_urls, gurl_1));
  EXPECT_FALSE(base::Contains(*handler_1_urls, gurl_2));

  std::vector<GURL>* handler_2_urls = handler_2.seen_urls();
  EXPECT_TRUE(base::Contains(*handler_2_urls, gurl_2));
  EXPECT_FALSE(base::Contains(*handler_2_urls, gurl_1));

  // Add a browser for each profile.
  std::unique_ptr<BrowserWindow> window_3(CreateBrowserWindow());
  std::unique_ptr<BrowserWindow> window_4(CreateBrowserWindow());

  std::unique_ptr<Browser> new_browser_in_first_profile(
      CreateBrowser(profile_1, browser()->type(), false, window_3.get()));
  std::unique_ptr<Browser> new_browser_in_second_profile(
      CreateBrowser(profile_2, browser()->type(), false, window_4.get()));

  GURL gurl_3("http://foo3.com");
  GURL gurl_4("http://foo4.com");
  AddTab(new_browser_in_first_profile.get(), gurl_3);
  AddTab(new_browser_in_second_profile.get(), gurl_4);

  handler_1_urls = handler_1.seen_urls();
  EXPECT_TRUE(base::Contains(*handler_1_urls, gurl_3));
  EXPECT_FALSE(base::Contains(*handler_1_urls, gurl_4));

  handler_2_urls = handler_2.seen_urls();
  EXPECT_TRUE(base::Contains(*handler_2_urls, gurl_4));
  EXPECT_FALSE(base::Contains(*handler_2_urls, gurl_3));

  // Cleanup needed for manually created browsers so they don't complain about
  // having open tabs when destructing.
  browser_2->tab_strip_model()->CloseAllTabs();
  new_browser_in_first_profile->tab_strip_model()->CloseAllTabs();
  new_browser_in_second_profile->tab_strip_model()->CloseAllTabs();
}

// Added when fixing https://crbug.com/777745, ensure tab discards are observed.
TEST_F(BrowserListRouterHelperTest, NotifyOnDiscardTab) {
  TestingProfile* profile_1 = profile();
  TestingProfile* profile_2 =
      profile_manager()->CreateTestingProfile("testing_profile2");

  std::unique_ptr<BrowserWindow> window_2(CreateBrowserWindow());
  std::unique_ptr<Browser> browser_2(
      CreateBrowser(profile_2, browser()->type(), false, window_2.get()));

  SyncSessionsWebContentsRouterFactory::GetInstance()
      ->GetForProfile(profile_1)
      ->StartRoutingTo(&handler_1);
  SyncSessionsWebContentsRouterFactory::GetInstance()
      ->GetForProfile(profile_2)
      ->StartRoutingTo(&handler_2);

  GURL gurl_1("http://foo1.com");
  AddTab(browser(), gurl_1);

  // Tab needs to have been active to be found when discarding.
  BrowserList::GetInstance()->SetLastActive(browser());

  EXPECT_EQ(gurl_1, *handler_1.seen_urls()->rbegin());
  SessionID old_id = *handler_1.seen_ids()->rbegin();

  // Remove previous any observations from setup to make checking expectations
  // easier below.
  handler_1.seen_urls()->clear();
  handler_1.seen_ids()->clear();

  g_browser_process->GetTabManager()->DiscardTabByExtension(
      browser()->tab_strip_model()->GetWebContentsAt(0));

  // We're typically notified twice while discarding tabs. Once for the
  // destruction of the old web contents, and once for the new. This test case
  // is really trying to make sure the TabReplacedAt() method is called, which
  // is going to be invoked for the new web contents. We can tell it is the new
  // one by finding |gurl_1| for an id that is not |old_id|.
  bool found_new_id = false;
  for (size_t i = 0; i < handler_1.seen_ids()->size(); ++i) {
    if (handler_1.seen_ids()->at(i) != old_id &&
        handler_1.seen_urls()->at(i) == gurl_1) {
      found_new_id = true;
      break;
    }
  }
  EXPECT_TRUE(found_new_id);

  // And of course |profile_2| shouldn't have seen anything.
  EXPECT_EQ(0U, handler_2.seen_urls()->size());

  // Cleanup needed for manually created browsers so they don't complain about
  // having open tabs when destructing.
  browser_2->tab_strip_model()->CloseAllTabs();
}

}  // namespace sync_sessions
