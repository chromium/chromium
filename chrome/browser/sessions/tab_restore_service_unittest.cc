// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sessions/core/tab_restore_service_impl.h"

#include <stddef.h>

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/sessions/chrome_tab_restore_service_client.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/sessions/session_service_utils.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/chrome_render_view_test.h"
#include "chrome/test/base/testing_profile.h"
#include "components/history/core/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/sessions/content/content_live_tab.h"
#include "components/sessions/content/content_test_helper.h"
#include "components/sessions/core/serialized_navigation_entry_test_helper.h"
#include "components/sessions/core/session_types.h"
#include "components/sessions/core/tab_restore_service_observer.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/render_view_test.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

typedef sessions::TabRestoreService::Entry Entry;
typedef sessions::TabRestoreService::Tab Tab;
typedef sessions::TabRestoreService::Window Window;

using content::NavigationEntry;
using content::WebContentsTester;
using sessions::ContentTestHelper;
using sessions::SerializedNavigationEntry;
using sessions::SerializedNavigationEntryTestHelper;

// Create subclass that overrides TimeNow so that we can control the time used
// for closed tabs and windows.
class TabRestoreTimeFactory : public sessions::TabRestoreService::TimeFactory {
 public:
  TabRestoreTimeFactory() : time_(base::Time::Now()) {}

  ~TabRestoreTimeFactory() override {}

  base::Time TimeNow() override { return time_; }

 private:
  base::Time time_;
};

class TabRestoreServiceImplTest : public ChromeRenderViewHostTestHarness {
 public:
  TabRestoreServiceImplTest()
      : url1_("http://1"),
        url2_("http://2"),
        url3_("http://3"),
        user_agent_override_(
            "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/535.19"
            " (KHTML, like Gecko) Chrome/18.0.1025.45 Safari/535.19"),
        time_factory_(NULL),
        window_id_(SessionID::FromSerializedValue(1)),
        tab_id_(SessionID::FromSerializedValue(2)) {}

  ~TabRestoreServiceImplTest() override {}

  SessionID tab_id() const { return tab_id_; }
  SessionID window_id() const { return window_id_; }

 protected:
  enum {
    kMaxEntries = sessions::TabRestoreServiceHelper::kMaxEntries,
  };

  // testing::Test:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    live_tab_ = base::WrapUnique(new sessions::ContentLiveTab(web_contents()));
    time_factory_ = new TabRestoreTimeFactory();
    service_.reset(new sessions::TabRestoreServiceImpl(
        std::make_unique<ChromeTabRestoreServiceClient>(profile()),
        profile()->GetPrefs(), time_factory_));
  }

  void TearDown() override {
    service_->Shutdown();
    service_.reset();
    delete time_factory_;
    ChromeRenderViewHostTestHarness::TearDown();
  }

  sessions::TabRestoreService::Entries* mutable_entries() {
    return service_->mutable_entries();
  }

  void PruneEntries() { service_->PruneEntries(); }

  void AddThreeNavigations() {
    // Navigate to three URLs.
    NavigateAndCommit(url1_);
    NavigateAndCommit(url2_);
    NavigateAndCommit(url3_);
  }

  void NavigateToIndex(int index) {
    // Navigate back. We have to do this song and dance as NavigationController
    // isn't happy if you navigate immediately while going back.
    controller().GoToIndex(index);
    WebContentsTester::For(web_contents())->CommitPendingNavigation();
  }

  void RecreateService() {
    // Must set service to null first so that it is destroyed before the new
    // one is created.
    service_->Shutdown();
    content::RunAllTasksUntilIdle();
    service_.reset();
    service_.reset(new sessions::TabRestoreServiceImpl(
        std::make_unique<ChromeTabRestoreServiceClient>(profile()),
        profile()->GetPrefs(), time_factory_));
    SynchronousLoadTabsFromLastSession();
  }

  // Adds a window with one tab and url to the profile's session service.
  // If |pinned| is true, the tab is marked as pinned in the session service.
  void AddWindowWithOneTabToSessionService(bool pinned) {
    // Create new window / tab IDs so that these remain distinct.
    window_id_ = SessionID::NewUnique();
    tab_id_ = SessionID::NewUnique();

    SessionService* session_service =
        SessionServiceFactory::GetForProfile(profile());
    session_service->SetWindowType(window_id(), Browser::TYPE_NORMAL);
    session_service->SetTabWindow(window_id(), tab_id());
    session_service->SetTabIndexInWindow(window_id(), tab_id(), 0);
    session_service->SetSelectedTabInWindow(window_id(), 0);
    if (pinned)
      session_service->SetPinnedState(window_id(), tab_id(), true);
    session_service->UpdateTabNavigation(
        window_id(), tab_id(),
        ContentTestHelper::CreateNavigation(url1_.spec(), "title"));
  }

  // Creates a SessionService and assigns it to the Profile. The SessionService
  // is configured with a single window with a single tab pointing at url1_ by
  // way of AddWindowWithOneTabToSessionService. If |pinned| is true, the
  // tab is marked as pinned in the session service.
  void CreateSessionServiceWithOneWindow(bool pinned) {
    std::unique_ptr<SessionService> session_service(
        new SessionService(profile()));
    SessionServiceFactory::SetForTestProfile(profile(),
                                             std::move(session_service));

    AddWindowWithOneTabToSessionService(pinned);

    // Set this, otherwise previous session won't be loaded.
    profile()->set_last_session_exited_cleanly(false);
  }

  void SynchronousLoadTabsFromLastSession() {
    // Ensures that the load is complete before continuing.
    service_->LoadTabsFromLastSession();
    content::RunAllTasksUntilIdle();
  }

  sessions::LiveTab* live_tab() { return live_tab_.get(); }

  GURL url1_;
  GURL url2_;
  GURL url3_;
  std::string user_agent_override_;
  std::unique_ptr<sessions::LiveTab> live_tab_;
  std::unique_ptr<sessions::TabRestoreServiceImpl> service_;
  TabRestoreTimeFactory* time_factory_;
  SessionID window_id_;
  SessionID tab_id_;
};

namespace {

class TestTabRestoreServiceObserver
    : public sessions::TabRestoreServiceObserver {
 public:
  TestTabRestoreServiceObserver() : got_loaded_(false) {}

  void clear_got_loaded() { got_loaded_ = false; }
  bool got_loaded() const { return got_loaded_; }

  // TabRestoreServiceObserver:
  void TabRestoreServiceDestroyed(
      sessions::TabRestoreService* service) override {}
  void TabRestoreServiceLoaded(sessions::TabRestoreService* service) override {
    got_loaded_ = true;
  }

 private:
  // Was TabRestoreServiceLoaded() invoked?
  bool got_loaded_;

  DISALLOW_COPY_AND_ASSIGN(TestTabRestoreServiceObserver);
};

}  // namespace

TEST_F(TabRestoreServiceImplTest, Basic) {
  AddThreeNavigations();

  // Have the service record the tab.
  service_->CreateHistoricalTab(live_tab(), -1);

  // Make sure an entry was created.
  ASSERT_EQ(1U, service_->entries().size());

  // Make sure the entry matches.
  Entry* entry = service_->entries().front().get();
  ASSERT_EQ(sessions::TabRestoreService::TAB, entry->type);
  Tab* tab = static_cast<Tab*>(entry);
  EXPECT_FALSE(tab->pinned);
  EXPECT_TRUE(tab->extension_app_id.empty());
  ASSERT_EQ(3U, tab->navigations.size());
  EXPECT_TRUE(url1_ == tab->navigations[0].virtual_url());
  EXPECT_TRUE(url2_ == tab->navigations[1].virtual_url());
  EXPECT_TRUE(url3_ == tab->navigations[2].virtual_url());
  EXPECT_EQ("", tab->user_agent_override);
  EXPECT_EQ(2, tab->current_navigation_index);
  EXPECT_EQ(
      time_factory_->TimeNow().ToDeltaSinceWindowsEpoch().InMicroseconds(),
      tab->timestamp.ToDeltaSinceWindowsEpoch().InMicroseconds());

  NavigateToIndex(1);

  // And check again, but set the user agent override this time.
  web_contents()->SetUserAgentOverride(user_agent_override_, false);
  service_->CreateHistoricalTab(live_tab(), -1);

  // There should be two entries now.
  ASSERT_EQ(2U, service_->entries().size());

  // Make sure the entry matches.
  entry = service_->entries().front().get();
  ASSERT_EQ(sessions::TabRestoreService::TAB, entry->type);
  tab = static_cast<Tab*>(entry);
  EXPECT_FALSE(tab->pinned);
  ASSERT_EQ(3U, tab->navigations.size());
  EXPECT_EQ(url1_, tab->navigations[0].virtual_url());
  EXPECT_EQ(url2_, tab->navigations[1].virtual_url());
  EXPECT_EQ(url3_, tab->navigations[2].virtual_url());
  EXPECT_EQ(user_agent_override_, tab->user_agent_override);
  EXPECT_EQ(1, tab->current_navigation_index);
  EXPECT_EQ(
      time_factory_->TimeNow().ToDeltaSinceWindowsEpoch().InMicroseconds(),
      tab->timestamp.ToDeltaSinceWindowsEpoch().InMicroseconds());
}

// Make sure TabRestoreService doesn't create an entry for a tab with no
// navigations.
TEST_F(TabRestoreServiceImplTest, DontCreateEmptyTab) {
  service_->CreateHistoricalTab(live_tab(), -1);
  EXPECT_TRUE(service_->entries().empty());
}

// Tests restoring a single tab.
TEST_F(TabRestoreServiceImplTest, Restore) {
  AddThreeNavigations();

  // Have the service record the tab.
  service_->CreateHistoricalTab(live_tab(), -1);

  // Recreate the service and have it load the tabs.
  RecreateService();

  // One entry should be created.
  ASSERT_EQ(1U, service_->entries().size());

  // And verify the entry.
  Entry* entry = service_->entries().front().get();
  ASSERT_EQ(sessions::TabRestoreService::TAB, entry->type);
  Tab* tab = static_cast<Tab*>(entry);
  EXPECT_FALSE(tab->pinned);
  ASSERT_EQ(3U, tab->navigations.size());
  EXPECT_TRUE(url1_ == tab->navigations[0].virtual_url());
  EXPECT_TRUE(url2_ == tab->navigations[1].virtual_url());
  EXPECT_TRUE(url3_ == tab->navigations[2].virtual_url());
  EXPECT_EQ(2, tab->current_navigation_index);
  EXPECT_EQ(
      time_factory_->TimeNow().ToDeltaSinceWindowsEpoch().InMicroseconds(),
      tab->timestamp.ToDeltaSinceWindowsEpoch().InMicroseconds());
}

// Tests restoring a tab with more than gMaxPersistNavigationCount entries.
TEST_F(TabRestoreServiceImplTest, RestoreManyNavigations) {
  AddThreeNavigations();
  AddThreeNavigations();
  AddThreeNavigations();

  // Have the service record the tab.
  service_->CreateHistoricalTab(live_tab(), -1);

  // Recreate the service and have it load the tabs.
  RecreateService();

  // One entry should be created.
  ASSERT_EQ(1U, service_->entries().size());

  // And verify the entry.
  Entry* entry = service_->entries().front().get();
  ASSERT_EQ(sessions::TabRestoreService::TAB, entry->type);
  Tab* tab = static_cast<Tab*>(entry);
  // Only gMaxPersistNavigationCount + 1 (current navigation) are persisted.
  ASSERT_EQ(7U, tab->navigations.size());
  // Check that they are created with correct indices.
  EXPECT_EQ(0, tab->navigations[0].index());
  EXPECT_EQ(6, tab->navigations[6].index());
  EXPECT_EQ(6, tab->current_navigation_index);
}

// Tests restoring a single pinned tab.
TEST_F(TabRestoreServiceImplTest, RestorePinnedAndApp) {
  AddThreeNavigations();

  // Have the service record the tab.
  service_->CreateHistoricalTab(live_tab(), -1);

  // One entry should be created.
  ASSERT_EQ(1U, service_->entries().size());

  // We have to explicitly mark the tab as pinned as there is no browser for
  // these tests.
  Entry* entry = service_->entries().front().get();
  ASSERT_EQ(sessions::TabRestoreService::TAB, entry->type);
  Tab* tab = static_cast<Tab*>(entry);
  tab->pinned = true;
  const std::string extension_app_id("test");
  tab->extension_app_id = extension_app_id;

  // Recreate the service and have it load the tabs.
  RecreateService();

  // One entry should be created.
  ASSERT_EQ(1U, service_->entries().size());

  // And verify the entry.
  entry = service_->entries().front().get();
  ASSERT_EQ(sessions::TabRestoreService::TAB, entry->type);
  tab = static_cast<Tab*>(entry);
  EXPECT_TRUE(tab->pinned);
  ASSERT_EQ(3U, tab->navigations.size());
  EXPECT_TRUE(url1_ == tab->navigations[0].virtual_url());
  EXPECT_TRUE(url2_ == tab->navigations[1].virtual_url());
  EXPECT_TRUE(url3_ == tab->navigations[2].virtual_url());
  EXPECT_EQ(2, tab->current_navigation_index);
  EXPECT_TRUE(extension_app_id == tab->extension_app_id);
}

// Tests deleting entries.
TEST_F(TabRestoreServiceImplTest, DeleteNavigationEntries) {
  SynchronousLoadTabsFromLastSession();
  AddThreeNavigations();

  // Have the service record the tab.
  service_->CreateHistoricalTab(live_tab(), -1);

  service_->DeleteNavigationEntries(
      base::BindLambdaForTesting([&](const SerializedNavigationEntry& entry) {
        return entry.virtual_url() == url2_;
      }));

  // The entry should still exist but url2_ was removed and indices adjusted.
  ASSERT_EQ(1U, service_->entries().size());
  Entry* entry = service_->entries().front().get();
  ASSERT_EQ(sessions::TabRestoreService::TAB, entry->type);
  Tab* tab = static_cast<Tab*>(entry);
  ASSERT_EQ(2U, tab->navigations.size());
  EXPECT_EQ(url1_, tab->navigations[0].virtual_url());
  EXPECT_EQ(0, tab->navigations[0].index());
  EXPECT_EQ(url3_, tab->navigations[1].virtual_url());
  EXPECT_EQ(1, tab->navigations[1].index());
  EXPECT_EQ(1, tab->current_navigation_index);

  service_->DeleteNavigationEntries(base::BindRepeating(
      [](const SerializedNavigationEntry& entry) { return true; }));

  // The entry should be removed.
  EXPECT_EQ(0U, service_->entries().size());
}

// Tests deleting entries.
TEST_F(TabRestoreServiceImplTest, DeleteCurrentEntry) {
  SynchronousLoadTabsFromLastSession();
  AddThreeNavigations();

  // Have the service record the tab.
  service_->CreateHistoricalTab(live_tab(), -1);

  service_->DeleteNavigationEntries(
      base::BindLambdaForTesting([&](const SerializedNavigationEntry& entry) {
        return entry.virtual_url() == url3_;
      }));

  // The entry should be deleted because the current url was deleted.
  EXPECT_EQ(0U, service_->entries().size());
}

// Tests deleting entries.
TEST_F(TabRestoreServiceImplTest, DeleteEntriesAndRecreate) {
  SynchronousLoadTabsFromLastSession();
  AddThreeNavigations();

  // Have the service record the tab.
  service_->CreateHistoricalTab(live_tab(), -1);

  // Delete the navigation for url2_.
  service_->DeleteNavigationEntries(
      base::BindLambdaForTesting([&](const SerializedNavigationEntry& entry) {
        return entry.virtual_url() == url2_;
      }));
  // Recreate the service and have it load the tabs.
  RecreateService();
  // The entry should still exist but url2_ was removed and indices adjusted.
  ASSERT_EQ(1U, service_->entries().size());
  Entry* entry = service_->entries().front().get();
  ASSERT_EQ(sessions::TabRestoreService::TAB, entry->type);
  Tab* tab = static_cast<Tab*>(entry);
  ASSERT_EQ(2U, tab->navigations.size());
  EXPECT_EQ(url1_, tab->navigations[0].virtual_url());
  EXPECT_EQ(0, tab->navigations[0].index());
  EXPECT_EQ(url3_, tab->navigations[1].virtual_url());
  EXPECT_EQ(1, tab->navigations[1].index());
  EXPECT_EQ(1, tab->current_navigation_index);

  // Delete all entries.
  service_->DeleteNavigationEntries(base::BindRepeating(
      [](const SerializedNavigationEntry& entry) { return true; }));
  // Recreate the service and have it load the tabs.
  RecreateService();
  // The entry should be removed.
  ASSERT_EQ(0U, service_->entries().size());
}

// Make sure we persist entries to disk that have post data.
TEST_F(TabRestoreServiceImplTest, DontPersistPostData) {
  AddThreeNavigations();
  controller().GetEntryAtIndex(0)->SetHasPostData(true);
  controller().GetEntryAtIndex(1)->SetHasPostData(true);
  controller().GetEntryAtIndex(2)->SetHasPostData(true);

  // Have the service record the tab.
  service_->CreateHistoricalTab(live_tab(), -1);
  ASSERT_EQ(1U, service_->entries().size());

  // Recreate the service and have it load the tabs.
  RecreateService();

  // One entry should be created.
  ASSERT_EQ(1U, service_->entries().size());

  const Entry* restored_entry = service_->entries().front().get();
  ASSERT_EQ(sessions::TabRestoreService::TAB, restored_entry->type);

  const Tab* restored_tab = static_cast<const Tab*>(restored_entry);
  // There should be 3 navs.
  ASSERT_EQ(3U, restored_tab->navigations.size());
  EXPECT_EQ(
      time_factory_->TimeNow().ToDeltaSinceWindowsEpoch().InMicroseconds(),
      restored_tab->timestamp.ToDeltaSinceWindowsEpoch().InMicroseconds());
}

// Make sure we don't persist entries to disk that have post data. This
// differs from DontPersistPostData1 in that all the navigations have post
// data, so that nothing should be persisted.
TEST_F(TabRestoreServiceImplTest, DontLoadTwice) {
  AddThreeNavigations();

  // Have the service record the tab.
  service_->CreateHistoricalTab(live_tab(), -1);
  ASSERT_EQ(1U, service_->entries().size());

  // Recreate the service and have it load the tabs.
  RecreateService();

  SynchronousLoadTabsFromLastSession();

  // There should only be one entry.
  ASSERT_EQ(1U, service_->entries().size());
}

// Makes sure we load the previous session as necessary.
TEST_F(TabRestoreServiceImplTest, LoadPreviousSession) {
  CreateSessionServiceWithOneWindow(false);

  SessionServiceFactory::GetForProfile(profile())
      ->MoveCurrentSessionToLastSession();

  EXPECT_FALSE(service_->IsLoaded());

  TestTabRestoreServiceObserver observer;
  service_->AddObserver(&observer);
  SynchronousLoadTabsFromLastSession();
  EXPECT_TRUE(observer.got_loaded());
  service_->RemoveObserver(&observer);

  // Make sure we get back one entry with one tab whose url is url1.
  ASSERT_EQ(1U, service_->entries().size());
  Entry* entry2 = service_->entries().front().get();
  ASSERT_EQ(sessions::TabRestoreService::WINDOW, entry2->type);
  sessions::TabRestoreService::Window* window =
      static_cast<sessions::TabRestoreService::Window*>(entry2);
  ASSERT_EQ(1U, window->tabs.size());
  EXPECT_EQ(0, window->timestamp.ToDeltaSinceWindowsEpoch().InMicroseconds());
  EXPECT_EQ(0, window->selected_tab_index);
  ASSERT_EQ(1U, window->tabs[0]->navigations.size());
  EXPECT_EQ(0, window->tabs[0]->current_navigation_index);
  EXPECT_EQ(
      0,
      window->tabs[0]->timestamp.ToDeltaSinceWindowsEpoch().InMicroseconds());
  EXPECT_TRUE(url1_ == window->tabs[0]->navigations[0].virtual_url());
}

// Makes sure we don't attempt to load previous sessions after a restore.
TEST_F(TabRestoreServiceImplTest, DontLoadAfterRestore) {
  CreateSessionServiceWithOneWindow(false);

  SessionServiceFactory::GetForProfile(profile())
      ->MoveCurrentSessionToLastSession();

  profile()->set_restored_last_session(true);

  SynchronousLoadTabsFromLastSession();

  // Because we restored a session TabRestoreService shouldn't load the tabs.
  ASSERT_EQ(0U, service_->entries().size());
}

// Makes sure we don't attempt to load previous sessions after a clean exit.
TEST_F(TabRestoreServiceImplTest, DontLoadAfterCleanExit) {
  CreateSessionServiceWithOneWindow(false);

  SessionServiceFactory::GetForProfile(profile())
      ->MoveCurrentSessionToLastSession();

  profile()->set_last_session_exited_cleanly(true);

  SynchronousLoadTabsFromLastSession();

  ASSERT_EQ(0U, service_->entries().size());
}

// Makes sure we don't save sessions when saving history is disabled.
TEST_F(TabRestoreServiceImplTest, DontSaveWhenSavingIsDisabled) {
  profile()->GetPrefs()->SetBoolean(prefs::kSavingBrowserHistoryDisabled, true);

  CreateSessionServiceWithOneWindow(false);

  SessionServiceFactory::GetForProfile(profile())
      ->MoveCurrentSessionToLastSession();

  SynchronousLoadTabsFromLastSession();

  ASSERT_EQ(0U, service_->entries().size());
}

// Makes sure we don't attempt to load previous sessions when saving history is
// disabled.
TEST_F(TabRestoreServiceImplTest, DontLoadWhenSavingIsDisabled) {
  CreateSessionServiceWithOneWindow(false);

  SessionServiceFactory::GetForProfile(profile())
      ->MoveCurrentSessionToLastSession();

  profile()->GetPrefs()->SetBoolean(prefs::kSavingBrowserHistoryDisabled, true);

  SynchronousLoadTabsFromLastSession();

  ASSERT_EQ(0U, service_->entries().size());
}

TEST_F(TabRestoreServiceImplTest, LoadPreviousSessionAndTabs) {
  CreateSessionServiceWithOneWindow(false);

  SessionServiceFactory::GetForProfile(profile())
      ->MoveCurrentSessionToLastSession();

  AddThreeNavigations();

  service_->CreateHistoricalTab(live_tab(), -1);

  RecreateService();

  // We should get back two entries, one from the previous session and one from
  // the tab restore service. The previous session entry should be first.
  ASSERT_EQ(2U, service_->entries().size());
  // The first entry should come from the session service.
  Entry* entry = service_->entries().front().get();
  ASSERT_EQ(sessions::TabRestoreService::WINDOW, entry->type);
  sessions::TabRestoreService::Window* window =
      static_cast<sessions::TabRestoreService::Window*>(entry);
  ASSERT_EQ(1U, window->tabs.size());
  EXPECT_EQ(0, window->selected_tab_index);
  EXPECT_EQ(0, window->timestamp.ToDeltaSinceWindowsEpoch().InMicroseconds());
  ASSERT_EQ(1U, window->tabs[0]->navigations.size());
  EXPECT_EQ(0, window->tabs[0]->current_navigation_index);
  EXPECT_EQ(
      0,
      window->tabs[0]->timestamp.ToDeltaSinceWindowsEpoch().InMicroseconds());
  EXPECT_TRUE(url1_ == window->tabs[0]->navigations[0].virtual_url());

  // Then the closed tab.
  entry = (++service_->entries().begin())->get();
  ASSERT_EQ(sessions::TabRestoreService::TAB, entry->type);
  Tab* tab = static_cast<Tab*>(entry);
  ASSERT_FALSE(tab->pinned);
  ASSERT_EQ(3U, tab->navigations.size());
  EXPECT_EQ(2, tab->current_navigation_index);
  EXPECT_EQ(
      time_factory_->TimeNow().ToDeltaSinceWindowsEpoch().InMicroseconds(),
      tab->timestamp.ToDeltaSinceWindowsEpoch().InMicroseconds());
  EXPECT_TRUE(url1_ == tab->navigations[0].virtual_url());
  EXPECT_TRUE(url2_ == tab->navigations[1].virtual_url());
  EXPECT_TRUE(url3_ == tab->navigations[2].virtual_url());
}

// Make sure window bounds and workspace are properly loaded from the session
// service.
TEST_F(TabRestoreServiceImplTest, LoadWindowBoundsAndWorkspace) {
  constexpr gfx::Rect kBounds(10, 20, 640, 480);
  constexpr ui::WindowShowState kShowState = ui::SHOW_STATE_MINIMIZED;
  constexpr char kWorkspace[] = "workspace";

  CreateSessionServiceWithOneWindow(false);

  // Set the bounds, show state and workspace.
  SessionService* session_service =
      SessionServiceFactory::GetForProfile(profile());
  session_service->SetWindowBounds(window_id(), kBounds, kShowState);
  session_service->SetWindowWorkspace(window_id(), kWorkspace);

  session_service->MoveCurrentSessionToLastSession();

  AddThreeNavigations();

  service_->CreateHistoricalTab(live_tab(), -1);

  RecreateService();

  // We should get back two entries, one from the previous session and one from
  // the tab restore service. The previous session entry should be first.
  ASSERT_EQ(2U, service_->entries().size());

  // The first entry should come from the session service.
  Entry* entry = service_->entries().front().get();
  ASSERT_EQ(sessions::TabRestoreService::WINDOW, entry->type);
  sessions::TabRestoreService::Window* window =
      static_cast<sessions::TabRestoreService::Window*>(entry);
  ASSERT_EQ(kBounds, window->bounds);
  ASSERT_EQ(kShowState, window->show_state);
  ASSERT_EQ(kWorkspace, window->workspace);
  ASSERT_EQ(1U, window->tabs.size());
  EXPECT_EQ(0, window->selected_tab_index);
  EXPECT_FALSE(window->tabs[0]->pinned);
  ASSERT_EQ(1U, window->tabs[0]->navigations.size());
  EXPECT_EQ(0, window->tabs[0]->current_navigation_index);
  EXPECT_TRUE(url1_ == window->tabs[0]->navigations[0].virtual_url());

  // Then the closed tab.
  entry = (++service_->entries().begin())->get();
  ASSERT_EQ(sessions::TabRestoreService::TAB, entry->type);
  Tab* tab = static_cast<Tab*>(entry);
  ASSERT_FALSE(tab->pinned);
  ASSERT_EQ(3U, tab->navigations.size());
  EXPECT_EQ(2, tab->current_navigation_index);
  EXPECT_TRUE(url1_ == tab->navigations[0].virtual_url());
  EXPECT_TRUE(url2_ == tab->navigations[1].virtual_url());
  EXPECT_TRUE(url3_ == tab->navigations[2].virtual_url());
}

// Make sure pinned state is correctly loaded from session service.
TEST_F(TabRestoreServiceImplTest, LoadPreviousSessionAndTabsPinned) {
  CreateSessionServiceWithOneWindow(true);

  SessionServiceFactory::GetForProfile(profile())
      ->MoveCurrentSessionToLastSession();

  AddThreeNavigations();

  service_->CreateHistoricalTab(live_tab(), -1);

  RecreateService();

  // We should get back two entries, one from the previous session and one from
  // the tab restore service. The previous session entry should be first.
  ASSERT_EQ(2U, service_->entries().size());
  // The first entry should come from the session service.
  Entry* entry = service_->entries().front().get();
  ASSERT_EQ(sessions::TabRestoreService::WINDOW, entry->type);
  sessions::TabRestoreService::Window* window =
      static_cast<sessions::TabRestoreService::Window*>(entry);
  ASSERT_EQ(1U, window->tabs.size());
  EXPECT_EQ(0, window->selected_tab_index);
  EXPECT_TRUE(window->tabs[0]->pinned);
  ASSERT_EQ(1U, window->tabs[0]->navigations.size());
  EXPECT_EQ(0, window->tabs[0]->current_navigation_index);
  EXPECT_TRUE(url1_ == window->tabs[0]->navigations[0].virtual_url());

  // Then the closed tab.
  entry = (++service_->entries().begin())->get();
  ASSERT_EQ(sessions::TabRestoreService::TAB, entry->type);
  Tab* tab = static_cast<Tab*>(entry);
  ASSERT_FALSE(tab->pinned);
  ASSERT_EQ(3U, tab->navigations.size());
  EXPECT_EQ(2, tab->current_navigation_index);
  EXPECT_TRUE(url1_ == tab->navigations[0].virtual_url());
  EXPECT_TRUE(url2_ == tab->navigations[1].virtual_url());
  EXPECT_TRUE(url3_ == tab->navigations[2].virtual_url());
}

// Creates kMaxEntries + 1 windows in the session service and makes sure we only
// get back kMaxEntries on restore.
TEST_F(TabRestoreServiceImplTest, ManyWindowsInSessionService) {
  CreateSessionServiceWithOneWindow(false);

  for (size_t i = 0; i < kMaxEntries; ++i)
    AddWindowWithOneTabToSessionService(false);

  SessionServiceFactory::GetForProfile(profile())
      ->MoveCurrentSessionToLastSession();

  AddThreeNavigations();

  service_->CreateHistoricalTab(live_tab(), -1);

  RecreateService();

  // We should get back kMaxEntries entries. We added more, but
  // TabRestoreService only allows up to kMaxEntries.
  ASSERT_EQ(static_cast<size_t>(kMaxEntries), service_->entries().size());

  // The first entry should come from the session service.
  Entry* entry = service_->entries().front().get();
  ASSERT_EQ(sessions::TabRestoreService::WINDOW, entry->type);
  sessions::TabRestoreService::Window* window =
      static_cast<sessions::TabRestoreService::Window*>(entry);
  ASSERT_EQ(1U, window->tabs.size());
  EXPECT_EQ(0, window->selected_tab_index);
  EXPECT_EQ(0, window->timestamp.ToDeltaSinceWindowsEpoch().InMicroseconds());
  ASSERT_EQ(1U, window->tabs[0]->navigations.size());
  EXPECT_EQ(0, window->tabs[0]->current_navigation_index);
  EXPECT_EQ(
      0,
      window->tabs[0]->timestamp.ToDeltaSinceWindowsEpoch().InMicroseconds());
  EXPECT_TRUE(url1_ == window->tabs[0]->navigations[0].virtual_url());
}

// Makes sure we restore timestamps correctly.
TEST_F(TabRestoreServiceImplTest, TimestampSurvivesRestore) {
  base::Time tab_timestamp(base::Time::FromDeltaSinceWindowsEpoch(
      base::TimeDelta::FromMicroseconds(123456789)));

  AddThreeNavigations();

  // Have the service record the tab.
  service_->CreateHistoricalTab(live_tab(), -1);

  // Make sure an entry was created.
  ASSERT_EQ(1U, service_->entries().size());

  // Make sure the entry matches.
  std::vector<SerializedNavigationEntry> old_navigations;
  {
    // |entry|/|tab| doesn't survive after RecreateService().
    Entry* entry = service_->entries().front().get();
    ASSERT_EQ(sessions::TabRestoreService::TAB, entry->type);
    Tab* tab = static_cast<Tab*>(entry);
    tab->timestamp = tab_timestamp;
    old_navigations = tab->navigations;
  }

  EXPECT_EQ(3U, old_navigations.size());
  for (size_t i = 0; i < old_navigations.size(); ++i) {
    EXPECT_FALSE(old_navigations[i].timestamp().is_null());
  }

  // Set this, otherwise previous session won't be loaded.
  profile()->set_last_session_exited_cleanly(false);

  RecreateService();

  // One entry should be created.
  ASSERT_EQ(1U, service_->entries().size());

  // And verify the entry.
  Entry* restored_entry = service_->entries().front().get();
  ASSERT_EQ(sessions::TabRestoreService::TAB, restored_entry->type);
  Tab* restored_tab = static_cast<Tab*>(restored_entry);
  EXPECT_EQ(
      tab_timestamp.ToDeltaSinceWindowsEpoch().InMicroseconds(),
      restored_tab->timestamp.ToDeltaSinceWindowsEpoch().InMicroseconds());
  ASSERT_EQ(old_navigations.size(), restored_tab->navigations.size());
  for (size_t i = 0; i < restored_tab->navigations.size(); ++i) {
    EXPECT_EQ(old_navigations[i].timestamp(),
              restored_tab->navigations[i].timestamp());
  }
}

// Makes sure we restore status codes correctly.
TEST_F(TabRestoreServiceImplTest, StatusCodesSurviveRestore) {
  AddThreeNavigations();

  // Have the service record the tab.
  service_->CreateHistoricalTab(live_tab(), -1);

  // Make sure an entry was created.
  ASSERT_EQ(1U, service_->entries().size());

  // Make sure the entry matches.
  std::vector<sessions::SerializedNavigationEntry> old_navigations;
  {
    // |entry|/|tab| doesn't survive after RecreateService().
    Entry* entry = service_->entries().front().get();
    ASSERT_EQ(sessions::TabRestoreService::TAB, entry->type);
    Tab* tab = static_cast<Tab*>(entry);
    old_navigations = tab->navigations;
  }

  EXPECT_EQ(3U, old_navigations.size());
  for (size_t i = 0; i < old_navigations.size(); ++i) {
    EXPECT_EQ(200, old_navigations[i].http_status_code());
  }

  // Set this, otherwise previous session won't be loaded.
  profile()->set_last_session_exited_cleanly(false);

  RecreateService();

  // One entry should be created.
  ASSERT_EQ(1U, service_->entries().size());

  // And verify the entry.
  Entry* restored_entry = service_->entries().front().get();
  ASSERT_EQ(sessions::TabRestoreService::TAB, restored_entry->type);
  Tab* restored_tab = static_cast<Tab*>(restored_entry);
  ASSERT_EQ(old_navigations.size(), restored_tab->navigations.size());
  for (size_t i = 0; i < restored_tab->navigations.size(); ++i) {
    EXPECT_EQ(200, restored_tab->navigations[i].http_status_code());
  }
}

TEST_F(TabRestoreServiceImplTest, PruneEntries) {
  service_->ClearEntries();
  ASSERT_TRUE(service_->entries().empty());

  const size_t max_entries = kMaxEntries;
  for (size_t i = 0; i < max_entries + 5; i++) {
    SerializedNavigationEntry navigation = ContentTestHelper::CreateNavigation(
        base::StringPrintf("http://%d", static_cast<int>(i)),
        base::NumberToString(i));

    auto tab = std::make_unique<Tab>();
    tab->navigations.push_back(navigation);
    tab->current_navigation_index = 0;

    mutable_entries()->push_back(std::move(tab));
  }

  // Only keep kMaxEntries around.
  EXPECT_EQ(max_entries + 5, service_->entries().size());
  PruneEntries();
  EXPECT_EQ(max_entries, service_->entries().size());
  // Pruning again does nothing.
  PruneEntries();
  EXPECT_EQ(max_entries, service_->entries().size());

  // Prune older first.
  const char kRecentUrl[] = "http://recent";
  SerializedNavigationEntry navigation =
      ContentTestHelper::CreateNavigation(kRecentUrl, "Most recent");
  auto tab = std::make_unique<Tab>();
  tab->navigations.push_back(navigation);
  tab->current_navigation_index = 0;
  mutable_entries()->push_front(std::move(tab));
  EXPECT_EQ(max_entries + 1, service_->entries().size());
  PruneEntries();
  EXPECT_EQ(max_entries, service_->entries().size());
  EXPECT_EQ(GURL(kRecentUrl), static_cast<Tab&>(*service_->entries().front())
                                  .navigations[0]
                                  .virtual_url());

  // Ignore NTPs.
  navigation = ContentTestHelper::CreateNavigation(chrome::kChromeUINewTabURL,
                                                   "New tab");

  tab = std::make_unique<Tab>();
  tab->navigations.push_back(navigation);
  tab->current_navigation_index = 0;
  mutable_entries()->push_front(std::move(tab));

  EXPECT_EQ(max_entries + 1, service_->entries().size());
  PruneEntries();
  EXPECT_EQ(max_entries, service_->entries().size());
  EXPECT_EQ(GURL(kRecentUrl), static_cast<Tab&>(*service_->entries().front())
                                  .navigations[0]
                                  .virtual_url());

  // Don't prune pinned NTPs.
  tab = std::make_unique<Tab>();
  tab->pinned = true;
  tab->current_navigation_index = 0;
  tab->navigations.push_back(navigation);
  mutable_entries()->push_front(std::move(tab));
  EXPECT_EQ(max_entries + 1, service_->entries().size());
  PruneEntries();
  EXPECT_EQ(max_entries, service_->entries().size());
  EXPECT_EQ(GURL(chrome::kChromeUINewTabURL),
            static_cast<Tab*>(service_->entries().front().get())
                ->navigations[0]
                .virtual_url());

  // Don't prune NTPs that have multiple navigations.
  // (Erase the last NTP first.)
  mutable_entries()->erase(mutable_entries()->begin());
  tab = std::make_unique<Tab>();
  tab->current_navigation_index = 1;
  tab->navigations.push_back(navigation);
  tab->navigations.push_back(navigation);
  mutable_entries()->push_front(std::move(tab));
  EXPECT_EQ(max_entries, service_->entries().size());
  PruneEntries();
  EXPECT_EQ(max_entries, service_->entries().size());
  EXPECT_EQ(GURL(chrome::kChromeUINewTabURL),
            static_cast<Tab*>(service_->entries().front().get())
                ->navigations[1]
                .virtual_url());
}

// Regression test for crbug.com/106082
TEST_F(TabRestoreServiceImplTest, PruneIsCalled) {
  CreateSessionServiceWithOneWindow(false);

  SessionServiceFactory::GetForProfile(profile())
      ->MoveCurrentSessionToLastSession();

  profile()->set_restored_last_session(true);

  const size_t max_entries = kMaxEntries;
  for (size_t i = 0; i < max_entries + 5; i++) {
    NavigateAndCommit(
        GURL(base::StringPrintf("http://%d", static_cast<int>(i))));
    service_->CreateHistoricalTab(live_tab(), -1);
  }

  EXPECT_EQ(max_entries, service_->entries().size());
  // This should not crash.
  SynchronousLoadTabsFromLastSession();
  EXPECT_EQ(max_entries, service_->entries().size());
}

// Makes sure invoking LoadTabsFromLastSession() when the max number of entries
// have been added results in IsLoaded() returning true and notifies observers.
TEST_F(TabRestoreServiceImplTest, GoToLoadedWhenHaveMaxEntries) {
  const size_t max_entries = kMaxEntries;
  for (size_t i = 0; i < max_entries + 5; i++) {
    NavigateAndCommit(
        GURL(base::StringPrintf("http://%d", static_cast<int>(i))));
    service_->CreateHistoricalTab(live_tab(), -1);
  }

  EXPECT_FALSE(service_->IsLoaded());
  TestTabRestoreServiceObserver observer;
  service_->AddObserver(&observer);
  EXPECT_EQ(max_entries, service_->entries().size());
  SynchronousLoadTabsFromLastSession();
  EXPECT_TRUE(observer.got_loaded());
  EXPECT_TRUE(service_->IsLoaded());
  service_->RemoveObserver(&observer);
}
