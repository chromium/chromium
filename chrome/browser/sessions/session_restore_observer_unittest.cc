// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/session_restore_observer.h"

#include <memory>
#include <optional>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/time/time.h"
#include "chrome/browser/performance_manager/public/background_tab_loading_policy.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/sessions/session_restore_delegate.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/test_support/page_node_utils.h"
#include "components/performance_manager/test_support/test_harness_helper.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/restore_type.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using content::WebContentsTester;
using performance_manager::PageNode;

namespace {

const char kDefaultUrl[] = "https://www.google.com";

enum class SessionRestoreEvent { kStartedLoadingTabs, kFinishedLoadingTabs };

class MockSessionRestoreObserver : public SessionRestoreObserver {
 public:
  MockSessionRestoreObserver() { SessionRestore::AddObserver(this); }

  MockSessionRestoreObserver(const MockSessionRestoreObserver&) = delete;
  MockSessionRestoreObserver& operator=(const MockSessionRestoreObserver&) =
      delete;

  ~MockSessionRestoreObserver() { SessionRestore::RemoveObserver(this); }

  const std::vector<SessionRestoreEvent>& session_restore_events() const {
    return session_restore_events_;
  }

  // SessionRestoreObserver implementation:
  void OnSessionRestoreStartedLoadingTabs() override {
    session_restore_events_.emplace_back(
        SessionRestoreEvent::kStartedLoadingTabs);
  }
  void OnSessionRestoreFinishedLoadingTabs() override {
    session_restore_events_.emplace_back(
        SessionRestoreEvent::kFinishedLoadingTabs);
  }

 private:
  std::vector<SessionRestoreEvent> session_restore_events_;
};

}  // namespace

class SessionRestoreObserverTest : public ChromeRenderViewHostTestHarness {
 public:
  using RestoredTab = SessionRestoreDelegate::RestoredTab;

  // testing::Test:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    pm_helper_.SetUp();
    performance_manager::policies::InstallBackgroundTabLoadingPolicyForTesting(
        base::BindRepeating(&SessionRestore::OnTabLoaderFinishedLoadingTabs));
    SetContents(CreateRestoredWebContents());
  }

  void TearDown() override {
    pm_helper_.TearDown();
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

    // In production the PageType is set when the WebContents is added to a
    // tab strip.
    performance_manager::testing::SetPageNodeType(
        performance_manager::testing::GetPageNodeForWebContents(
            test_contents.get()),
        performance_manager::PageType::kTab);

    return test_contents;
  }

  void RestoreTabs(std::vector<content::WebContents*> tabs) {
    std::vector<RestoredTab> restored_tabs;
    for (content::WebContents* web_contents : tabs) {
      restored_tabs.emplace_back(web_contents, false, false, false,
                                 std::nullopt, std::nullopt);
    }
    SessionRestoreDelegate::RestoreTabs(restored_tabs, base::TimeTicks::Now());
  }

  void LoadWebContents(content::WebContents* contents) {
    WebContentsTester::For(contents)->NavigateAndCommit(GURL(kDefaultUrl));
    WebContentsTester::For(contents)->TestSetIsLoading(false);
    // Transition through LOADING to LOADED to advance the loading scheduler.
    auto* page_node =
        performance_manager::testing::GetPageNodeForWebContents(contents);
    if (page_node->GetLoadingState() != PageNode::LoadingState::kLoading) {
      performance_manager::testing::SetPageNodeLoadingState(
          page_node, PageNode::LoadingState::kLoading);
    }
    performance_manager::testing::SetPageNodeLoadingState(
        page_node, PageNode::LoadingState::kLoadedIdle);
  }

  const std::vector<SessionRestoreEvent>& session_restore_events() const {
    return mock_observer_.session_restore_events();
  }

  size_t number_of_session_restore_events() const {
    return session_restore_events().size();
  }

 private:
  performance_manager::PerformanceManagerTestHarnessHelper pm_helper_;

  MockSessionRestoreObserver mock_observer_;
};

TEST_F(SessionRestoreObserverTest, SingleSessionRestore) {
  SessionRestore::NotifySessionRestoreStartedLoadingTabs();
  RestoreTabs({web_contents()});

  ASSERT_EQ(1u, number_of_session_restore_events());
  EXPECT_EQ(SessionRestoreEvent::kStartedLoadingTabs,
            session_restore_events()[0]);

  LoadWebContents(web_contents());

  ASSERT_EQ(2u, number_of_session_restore_events());
  EXPECT_EQ(SessionRestoreEvent::kFinishedLoadingTabs,
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
    EXPECT_EQ(SessionRestoreEvent::kStartedLoadingTabs,
              session_restore_events()[event_index++]);

    LoadWebContents(test_contents);
    ASSERT_EQ(event_index + 1, number_of_session_restore_events());
    EXPECT_EQ(SessionRestoreEvent::kFinishedLoadingTabs,
              session_restore_events()[event_index++]);
  }
}

TEST_F(SessionRestoreObserverTest, ConcurrentSessionRestores) {
  auto test_contents = CreateRestoredWebContents();

  SessionRestore::NotifySessionRestoreStartedLoadingTabs();
  RestoreTabs({web_contents()});
  RestoreTabs({test_contents.get()});

  ASSERT_EQ(1u, number_of_session_restore_events());
  EXPECT_EQ(SessionRestoreEvent::kStartedLoadingTabs,
            session_restore_events()[0]);

  LoadWebContents(web_contents());
  LoadWebContents(test_contents.get());
  ASSERT_EQ(2u, number_of_session_restore_events());
  EXPECT_EQ(SessionRestoreEvent::kFinishedLoadingTabs,
            session_restore_events()[1]);
}
