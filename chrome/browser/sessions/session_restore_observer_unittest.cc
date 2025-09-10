// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/session_restore_observer.h"

#include <optional>
#include <set>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/performance_manager/public/background_tab_loading_policy.h"
#include "chrome/browser/resource_coordinator/tab_helper.h"
#include "chrome/browser/resource_coordinator/tab_load_tracker.h"
#include "chrome/browser/resource_coordinator/tab_manager.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/sessions/session_restore_stats_collector.h"
#include "chrome/browser/sessions/tab_loader.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/test_support/page_node_utils.h"
#include "components/performance_manager/test_support/test_harness_helper.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/web_contents_tester.h"

using content::WebContentsTester;
using performance_manager::PageNode;
using performance_manager::PerformanceManagerTestHarnessHelper;
using resource_coordinator::TabLoadTracker;
using LoadingState = TabLoadTracker::LoadingState;

namespace {

const char kDefaultUrl[] = "https://www.google.com";

}  // namespace

class MockSessionRestoreObserver : public SessionRestoreObserver {
 public:
  MockSessionRestoreObserver() { SessionRestore::AddObserver(this); }

  MockSessionRestoreObserver(const MockSessionRestoreObserver&) = delete;
  MockSessionRestoreObserver& operator=(const MockSessionRestoreObserver&) =
      delete;

  ~MockSessionRestoreObserver() { SessionRestore::RemoveObserver(this); }

  enum class SessionRestoreEvent {
    STARTED_LOADING_TABS,
    FINISHED_LOADING_TABS
  };

  const std::vector<SessionRestoreEvent>& session_restore_events() const {
    return session_restore_events_;
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

 private:
  std::vector<SessionRestoreEvent> session_restore_events_;
};

class SessionRestoreObserverTest : public ChromeRenderViewHostTestHarness {
 public:
  using RestoredTab = SessionRestoreDelegate::RestoredTab;

  SessionRestoreObserverTest() = default;

  SessionRestoreObserverTest(const SessionRestoreObserverTest&) = delete;
  SessionRestoreObserverTest& operator=(const SessionRestoreObserverTest&) =
      delete;

  // testing::Test:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    if (pm_helper_) {
      pm_helper_->SetUp();
      performance_manager::policies::
          InstallBackgroundTabLoadingPolicyForTesting(base::BindRepeating(
              &SessionRestore::OnTabLoaderFinishedLoadingTabs));
    }
    SetContents(CreateRestoredWebContents());
  }

  void TearDown() override {
    if (pm_helper_) {
      pm_helper_->TearDown();
    }
    ChromeRenderViewHostTestHarness::TearDown();
  }

 protected:
  std::unique_ptr<content::WebContents> CreateRestoredWebContents() {
    std::unique_ptr<content::WebContents> test_contents(
        WebContentsTester::CreateTestWebContents(browser_context(), nullptr));
    std::vector<std::unique_ptr<content::NavigationEntry>> entries;
    entries.push_back(content::NavigationEntry::Create());
    test_contents->GetController().Restore(0, content::RestoreType::kRestored,
                                           &entries);
    // TabLoadTracker needs the resource_coordinator WebContentsData to be
    // initialized, which is needed by TabLoader.
    resource_coordinator::ResourceCoordinatorTabHelper::CreateForWebContents(
        test_contents.get());
    if (pm_helper_) {
      // In production the PageType is set when the WebContents is added to a
      // tab strip.
      performance_manager::testing::SetPageNodeType(
          performance_manager::testing::GetPageNodeForWebContents(
              test_contents.get()),
          performance_manager::PageType::kTab);
    }
    return test_contents;
  }

  void RestoreTabs(std::vector<content::WebContents*> tabs) {
    if (pm_helper_) {
      performance_manager::policies::ScheduleLoadForRestoredTabs(
          std::move(tabs));
    } else {
      std::vector<RestoredTab> restored_tabs;
      for (content::WebContents* web_contents : tabs) {
        restored_tabs.emplace_back(web_contents, false, false, false,
                                   std::nullopt, std::nullopt);
      }
      TabLoader::DeprecatedRestoreTabs(std::move(restored_tabs),
                                       base::TimeTicks());
    }
  }

  void LoadWebContents(content::WebContents* contents) {
    WebContentsTester::For(contents)->NavigateAndCommit(GURL(kDefaultUrl));
    WebContentsTester::For(contents)->TestSetIsLoading(false);
    // Transition through LOADING to LOADED to advance the loading scheduler.
    if (pm_helper_) {
      auto* page_node =
          performance_manager::testing::GetPageNodeForWebContents(contents);
      if (page_node->GetLoadingState() != PageNode::LoadingState::kLoading) {
        performance_manager::testing::SetPageNodeLoadingState(
            page_node, PageNode::LoadingState::kLoading);
      }
      performance_manager::testing::SetPageNodeLoadingState(
          page_node, PageNode::LoadingState::kLoadedIdle);
    } else {
      if (TabLoadTracker::Get()->GetLoadingState(contents) !=
          LoadingState::LOADING) {
        TabLoadTracker::Get()->TransitionStateForTesting(contents,
                                                         LoadingState::LOADING);
      }
      TabLoadTracker::Get()->TransitionStateForTesting(contents,
                                                       LoadingState::LOADED);
    }
  }

  const std::vector<MockSessionRestoreObserver::SessionRestoreEvent>&
  session_restore_events() const {
    return mock_observer_.session_restore_events();
  }

  size_t number_of_session_restore_events() const {
    return session_restore_events().size();
  }

 private:
  // Only initialized if BackgroundTabLoadingFromPerformanceManager is enabled.
  std::optional<PerformanceManagerTestHarnessHelper> pm_helper_ =
      base::FeatureList::IsEnabled(
          performance_manager::features::
              kBackgroundTabLoadingFromPerformanceManager)
          ? std::make_optional<PerformanceManagerTestHarnessHelper>()
          : std::nullopt;

  MockSessionRestoreObserver mock_observer_;
};

TEST_F(SessionRestoreObserverTest, SingleSessionRestore) {
  SessionRestore::NotifySessionRestoreStartedLoadingTabs();
  RestoreTabs({web_contents()});

  ASSERT_EQ(1u, number_of_session_restore_events());
  EXPECT_EQ(
      MockSessionRestoreObserver::SessionRestoreEvent::STARTED_LOADING_TABS,
      session_restore_events()[0]);

  LoadWebContents(web_contents());

  ASSERT_EQ(2u, number_of_session_restore_events());
  EXPECT_EQ(
      MockSessionRestoreObserver::SessionRestoreEvent::FINISHED_LOADING_TABS,
      session_restore_events()[1]);
}

TEST_F(SessionRestoreObserverTest, SequentialSessionRestores) {
  const size_t number_of_session_restores = 3;
  size_t event_index = 0;
  std::vector<std::unique_ptr<content::WebContents>> different_test_contents;

  for (size_t i = 0; i < number_of_session_restores; ++i) {
    different_test_contents.emplace_back(CreateRestoredWebContents());
    content::WebContents* test_contents = different_test_contents.back().get();

    SessionRestore::NotifySessionRestoreStartedLoadingTabs();
    RestoreTabs({test_contents});

    ASSERT_EQ(event_index + 1, number_of_session_restore_events());
    EXPECT_EQ(
        MockSessionRestoreObserver::SessionRestoreEvent::STARTED_LOADING_TABS,
        session_restore_events()[event_index++]);

    LoadWebContents(test_contents);
    ASSERT_EQ(event_index + 1, number_of_session_restore_events());
    EXPECT_EQ(
        MockSessionRestoreObserver::SessionRestoreEvent::FINISHED_LOADING_TABS,
        session_restore_events()[event_index++]);
  }
}

TEST_F(SessionRestoreObserverTest, ConcurrentSessionRestores) {
  auto test_contents = CreateRestoredWebContents();

  SessionRestore::NotifySessionRestoreStartedLoadingTabs();
  RestoreTabs({web_contents()});
  RestoreTabs({test_contents.get()});

  ASSERT_EQ(1u, number_of_session_restore_events());
  EXPECT_EQ(
      MockSessionRestoreObserver::SessionRestoreEvent::STARTED_LOADING_TABS,
      session_restore_events()[0]);

  LoadWebContents(web_contents());
  LoadWebContents(test_contents.get());
  ASSERT_EQ(2u, number_of_session_restore_events());
  EXPECT_EQ(
      MockSessionRestoreObserver::SessionRestoreEvent::FINISHED_LOADING_TABS,
      session_restore_events()[1]);
}
