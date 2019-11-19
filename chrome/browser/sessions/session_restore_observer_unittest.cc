// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/session_restore_observer.h"

#include <set>
#include <vector>

#include "chrome/browser/browser_process.h"
#include "chrome/browser/resource_coordinator/tab_helper.h"
#include "chrome/browser/resource_coordinator/tab_load_tracker.h"
#include "chrome/browser/resource_coordinator/tab_manager.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/sessions/session_restore_stats_collector.h"
#include "chrome/browser/sessions/tab_loader.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/web_contents_tester.h"

using content::WebContentsTester;
using resource_coordinator::TabLoadTracker;
using LoadingState = TabLoadTracker::LoadingState;

namespace {

const char kDefaultUrl[] = "https://www.google.com";

}  // namespace

class MockSessionRestoreObserver : public SessionRestoreObserver {
 public:
  MockSessionRestoreObserver() { SessionRestore::AddObserver(this); }

  ~MockSessionRestoreObserver() { SessionRestore::RemoveObserver(this); }

  enum class SessionRestoreEvent {
    STARTED_LOADING_TABS,
    FINISHED_LOADING_TABS
  };

  const std::vector<SessionRestoreEvent>& session_restore_events() const {
    return session_restore_events_;
  }

  const std::set<content::WebContents*>& tabs_restoring() const {
    return tabs_restoring_;
  }

  // SessionRestoreObserver implementation:
  void OnSessionRestoreStartedLoadingTabs() override {
    session_restore_events_.emplace_back(
        SessionRestoreEvent::STARTED_LOADING_TABS);
  }
  void OnSessionRestoreFinishedLoadingTabs() override {
    session_restore_events_.emplace_back(
        SessionRestoreEvent::FINISHED_LOADING_TABS);
  }
  void OnWillRestoreTab(content::WebContents* contents) override {
    tabs_restoring_.emplace(contents);
  }

  void OnDidRestoreTab(content::WebContents* contents) {
    tabs_restoring_.erase(contents);
  }

 private:
  std::vector<SessionRestoreEvent> session_restore_events_;
  std::set<content::WebContents*> tabs_restoring_;

  DISALLOW_COPY_AND_ASSIGN(MockSessionRestoreObserver);
};

class SessionRestoreObserverTest : public ChromeRenderViewHostTestHarness {
 public:
  using RestoredTab = SessionRestoreDelegate::RestoredTab;

  SessionRestoreObserverTest() {}

  // testing::Test:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    SetContents(CreateRestoredWebContents());
    restored_tabs_.emplace_back(web_contents(), false, false, false,
                                base::nullopt);
  }

  void TearDown() override {
    ChromeRenderViewHostTestHarness::TearDown();
    restored_tabs_.clear();
  }

 protected:
  std::unique_ptr<content::WebContents> CreateRestoredWebContents() {
    std::unique_ptr<content::WebContents> test_contents(
        WebContentsTester::CreateTestWebContents(browser_context(), nullptr));
    std::vector<std::unique_ptr<content::NavigationEntry>> entries;
    entries.push_back(content::NavigationEntry::Create());
    test_contents->GetController().Restore(
        0, content::RestoreType::LAST_SESSION_EXITED_CLEANLY, &entries);
    // TabLoadTracker needs the resource_coordinator WebContentsData to be
    // initialized, which is needed by TabLoader.
    resource_coordinator::ResourceCoordinatorTabHelper::CreateForWebContents(
        test_contents.get());
    return test_contents;
  }

  void RestoreTabs() {
    TabLoader::RestoreTabs(restored_tabs_, base::TimeTicks());
  }

  void LoadWebContents(content::WebContents* contents) {
    WebContentsTester::For(contents)->NavigateAndCommit(GURL(kDefaultUrl));
    WebContentsTester::For(contents)->TestSetIsLoading(false);
    // Transition through LOADING to LOADED in order to keep the
    // SessionRestoreStatsCollector state machine happy.
    if (TabLoadTracker::Get()->GetLoadingState(contents) !=
        LoadingState::LOADING) {
      TabLoadTracker::Get()->TransitionStateForTesting(contents,
                                                       LoadingState::LOADING);
    }
    TabLoadTracker::Get()->TransitionStateForTesting(contents,
                                                     LoadingState::LOADED);
    mock_observer_.OnDidRestoreTab(contents);
  }

  const std::vector<MockSessionRestoreObserver::SessionRestoreEvent>&
  session_restore_events() const {
    return mock_observer_.session_restore_events();
  }

  size_t number_of_session_restore_events() const {
    return session_restore_events().size();
  }

  size_t number_of_tabs_restoring() const {
    return mock_observer_.tabs_restoring().size();
  }

 private:
  MockSessionRestoreObserver mock_observer_;
  std::vector<RestoredTab> restored_tabs_;

  DISALLOW_COPY_AND_ASSIGN(SessionRestoreObserverTest);
};

TEST_F(SessionRestoreObserverTest, SingleSessionRestore) {
  SessionRestore::NotifySessionRestoreStartedLoadingTabs();
  SessionRestore::OnWillRestoreTab(web_contents());
  RestoreTabs();

  ASSERT_EQ(1u, number_of_session_restore_events());
  EXPECT_EQ(
      MockSessionRestoreObserver::SessionRestoreEvent::STARTED_LOADING_TABS,
      session_restore_events()[0]);
  EXPECT_EQ(1u, number_of_tabs_restoring());

  LoadWebContents(web_contents());

  ASSERT_EQ(2u, number_of_session_restore_events());
  EXPECT_EQ(
      MockSessionRestoreObserver::SessionRestoreEvent::FINISHED_LOADING_TABS,
      session_restore_events()[1]);
  EXPECT_EQ(0u, number_of_tabs_restoring());
}

TEST_F(SessionRestoreObserverTest, SequentialSessionRestores) {
  const size_t number_of_session_restores = 3;
  size_t event_index = 0;
  std::vector<std::unique_ptr<content::WebContents>> different_test_contents;

  for (size_t i = 0; i < number_of_session_restores; ++i) {
    different_test_contents.emplace_back(CreateRestoredWebContents());
    content::WebContents* test_contents = different_test_contents.back().get();
    std::vector<RestoredTab> restored_tabs{
        RestoredTab(test_contents, false, false, false, base::nullopt)};

    SessionRestore::NotifySessionRestoreStartedLoadingTabs();
    SessionRestore::OnWillRestoreTab(test_contents);
    TabLoader::RestoreTabs(restored_tabs, base::TimeTicks());

    ASSERT_EQ(event_index + 1, number_of_session_restore_events());
    EXPECT_EQ(
        MockSessionRestoreObserver::SessionRestoreEvent::STARTED_LOADING_TABS,
        session_restore_events()[event_index++]);
    EXPECT_EQ(1u, number_of_tabs_restoring());

    LoadWebContents(test_contents);
    ASSERT_EQ(event_index + 1, number_of_session_restore_events());
    EXPECT_EQ(
        MockSessionRestoreObserver::SessionRestoreEvent::FINISHED_LOADING_TABS,
        session_restore_events()[event_index++]);
    EXPECT_EQ(0u, number_of_tabs_restoring());
  }
}

TEST_F(SessionRestoreObserverTest, ConcurrentSessionRestores) {
  std::vector<RestoredTab> another_restored_tabs;
  auto test_contents = CreateRestoredWebContents();
  another_restored_tabs.emplace_back(test_contents.get(), false, false, false,
                                     base::nullopt);

  SessionRestore::NotifySessionRestoreStartedLoadingTabs();
  SessionRestore::OnWillRestoreTab(web_contents());
  SessionRestore::OnWillRestoreTab(test_contents.get());
  RestoreTabs();
  TabLoader::RestoreTabs(another_restored_tabs, base::TimeTicks());

  ASSERT_EQ(1u, number_of_session_restore_events());
  EXPECT_EQ(
      MockSessionRestoreObserver::SessionRestoreEvent::STARTED_LOADING_TABS,
      session_restore_events()[0]);
  EXPECT_EQ(2u, number_of_tabs_restoring());

  LoadWebContents(web_contents());
  LoadWebContents(test_contents.get());
  ASSERT_EQ(2u, number_of_session_restore_events());
  EXPECT_EQ(
      MockSessionRestoreObserver::SessionRestoreEvent::FINISHED_LOADING_TABS,
      session_restore_events()[1]);
  EXPECT_EQ(0u, number_of_tabs_restoring());
}

TEST_F(SessionRestoreObserverTest, TabManagerShouldObserveSessionRestore) {
  auto test_contents = CreateRestoredWebContents();

  std::vector<SessionRestoreDelegate::RestoredTab> restored_tabs{
      SessionRestoreDelegate::RestoredTab(test_contents.get(), false, false,
                                          false, base::nullopt)};

  resource_coordinator::TabManager* tab_manager =
      g_browser_process->GetTabManager();
  EXPECT_FALSE(tab_manager->IsSessionRestoreLoadingTabs());
  EXPECT_FALSE(tab_manager->IsTabInSessionRestore(test_contents.get()));

  SessionRestore::NotifySessionRestoreStartedLoadingTabs();
  SessionRestore::OnWillRestoreTab(test_contents.get());
  EXPECT_TRUE(tab_manager->IsSessionRestoreLoadingTabs());
  EXPECT_TRUE(tab_manager->IsTabInSessionRestore(test_contents.get()));
  TabLoader::RestoreTabs(restored_tabs, base::TimeTicks());

  LoadWebContents(test_contents.get());
  EXPECT_FALSE(tab_manager->IsSessionRestoreLoadingTabs());
  EXPECT_FALSE(tab_manager->IsTabInSessionRestore(test_contents.get()));
}
