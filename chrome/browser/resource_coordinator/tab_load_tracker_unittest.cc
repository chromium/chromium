// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/tab_load_tracker.h"

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/process/kill.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/preloading/prefetch/no_state_prefetch/no_state_prefetch_manager_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_handle.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_manager.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace resource_coordinator {

using testing::_;
using testing::StrictMock;
using LoadingState = TabLoadTracker::LoadingState;

namespace {

void NavigateAndFinishLoading(content::WebContents* web_contents,
                              const GURL& url) {
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents, url);
}

std::unique_ptr<content::NavigationSimulator> NavigateAndKeepLoading(
    content::WebContents* web_contents,
    const GURL& url) {
  auto navigation =
      content::NavigationSimulator::CreateBrowserInitiated(url, web_contents);
  navigation->SetKeepLoading(true);
  navigation->Commit();
  return navigation;
}

}  // namespace

// Test wrapper of TabLoadTracker that exposes some internals.
class TestTabLoadTracker : public TabLoadTracker {
 public:
  using TabLoadTracker::DetermineLoadingState;
  using TabLoadTracker::OnPageStoppedLoading;
  using TabLoadTracker::PrimaryPageChanged;
  using TabLoadTracker::RenderProcessGone;
  using TabLoadTracker::StartTracking;
  using TabLoadTracker::StopTracking;

  TestTabLoadTracker() : all_tabs_are_non_ui_tabs_(false) {}
  virtual ~TestTabLoadTracker() {}

  // Some accessors for TabLoadTracker internals.
  const TabMap& tabs() const { return tabs_; }

  bool IsUiTab(content::WebContents* web_contents) override {
    if (all_tabs_are_non_ui_tabs_)
      return false;
    return TabLoadTracker::IsUiTab(web_contents);
  }

  void SetAllTabsAreNonUiTabs(bool enabled) {
    all_tabs_are_non_ui_tabs_ = enabled;
  }

 private:
  bool all_tabs_are_non_ui_tabs_;
};

// A mock observer class.
class LenientMockObserver : public TabLoadTracker::Observer {
 public:
  LenientMockObserver() {}

  LenientMockObserver(const LenientMockObserver&) = delete;
  LenientMockObserver& operator=(const LenientMockObserver&) = delete;

  ~LenientMockObserver() override {}

  // TabLoadTracker::Observer implementation:
  MOCK_METHOD2(OnStartTracking, void(content::WebContents*, LoadingState));
  MOCK_METHOD3(OnLoadingStateChange,
               void(content::WebContents*, LoadingState, LoadingState));
  MOCK_METHOD2(OnStopTracking, void(content::WebContents*, LoadingState));
};
using MockObserver = testing::StrictMock<LenientMockObserver>;

// A WebContentsObserver that forwards relevant WebContents events to the
// provided tracker.
class TestWebContentsObserver : public content::WebContentsObserver {
 public:
  TestWebContentsObserver(content::WebContents* web_contents,
                          TestTabLoadTracker* tracker)
      : content::WebContentsObserver(web_contents), tracker_(tracker) {}

  ~TestWebContentsObserver() override {}

  // content::WebContentsObserver:
  void PrimaryPageChanged(content::Page& page) override {
    tracker_->PrimaryPageChanged(web_contents());
  }
  void PrimaryMainFrameRenderProcessGone(
      base::TerminationStatus status) override {
    tracker_->RenderProcessGone(web_contents(), status);
  }

 private:
  raw_ptr<TestTabLoadTracker> tracker_;
};

// The test harness.
class TabLoadTrackerTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    contents1_ = CreateTestWebContents();
    contents2_ = CreateTestWebContents();
    contents3_ = CreateTestWebContents();

    tracker_.AddObserver(&observer_);
  }

  void TearDown() override {
    // The WebContents must be deleted before the test harness deletes the
    // RenderProcessHost.
    contents1_.reset();
    contents2_.reset();
    contents3_.reset();

    ChromeRenderViewHostTestHarness::TearDown();
  }

  void ExpectTabCounts(size_t tabs,
                       size_t unloaded,
                       size_t loading,
                       size_t loaded) {
    EXPECT_EQ(tabs, unloaded + loading + loaded);
    EXPECT_EQ(tabs, tracker().GetTabCount());
    EXPECT_EQ(unloaded, tracker().GetUnloadedTabCount());
    EXPECT_EQ(loading, tracker().GetLoadingTabCount());
    EXPECT_EQ(loaded, tracker().GetLoadedTabCount());
  }

  void ExpectUiTabCounts(size_t tabs,
                         size_t unloaded,
                         size_t loading,
                         size_t loaded) {
    EXPECT_EQ(tabs, unloaded + loading + loaded);
    EXPECT_EQ(tabs, tracker().GetUiTabCount());
    EXPECT_EQ(unloaded, tracker().GetUnloadedUiTabCount());
    EXPECT_EQ(loading, tracker().GetLoadingUiTabCount());
    EXPECT_EQ(loaded, tracker().GetLoadedUiTabCount());
  }

  void StateTransitionsTest(bool use_non_ui_tabs);

  TestTabLoadTracker& tracker() { return tracker_; }
  MockObserver& observer() { return observer_; }

  content::WebContents* contents1() { return contents1_.get(); }
  content::WebContents* contents2() { return contents2_.get(); }
  content::WebContents* contents3() { return contents3_.get(); }

 private:
  TestTabLoadTracker tracker_;
  MockObserver observer_;

  std::unique_ptr<content::WebContents> contents1_;
  std::unique_ptr<content::WebContents> contents2_;
  std::unique_ptr<content::WebContents> contents3_;
};

// A macro that ensures that a meaningful line number gets included in the
// stack trace when ExpectTabCounts fails.
#define EXPECT_TAB_COUNTS(a, b, c, d) \
  {                                   \
    SCOPED_TRACE("");                 \
    ExpectTabCounts(a, b, c, d);      \
  }
#define EXPECT_UI_TAB_COUNTS(a, b, c, d) \
  {                                      \
    SCOPED_TRACE("");                    \
    ExpectUiTabCounts(a, b, c, d);       \
  }
#define EXPECT_TAB_AND_UI_TAB_COUNTS(a, b, c, d) \
  {                                              \
    SCOPED_TRACE("");                            \
    ExpectTabCounts(a, b, c, d);                 \
    ExpectUiTabCounts(a, b, c, d);               \
  }

TEST_F(TabLoadTrackerTest, DetermineLoadingState) {
  EXPECT_EQ(LoadingState::UNLOADED,
            tracker().DetermineLoadingState(contents1()));

  // Navigate to a page and expect it to be loading.
  auto navigation =
      NavigateAndKeepLoading(contents1(), GURL("http://chromium.org"));
  EXPECT_EQ(LoadingState::LOADING,
            tracker().DetermineLoadingState(contents1()));

  // Indicate that loading is finished and expect the state to transition.
  navigation->StopLoading();
  EXPECT_EQ(LoadingState::LOADED, tracker().DetermineLoadingState(contents1()));
}

void TabLoadTrackerTest::StateTransitionsTest(bool use_non_ui_tabs) {
  tracker().SetAllTabsAreNonUiTabs(use_non_ui_tabs);

  // Set up the contents in UNLOADED, LOADING and LOADED states. This tests
  // each possible "entry" state.
  auto navigation_tab_2 =
      NavigateAndKeepLoading(contents2(), GURL("http://foo.com"));
  NavigateAndFinishLoading(contents3(), GURL("http://bar.com"));

  // Add the contents to the tracker.
  EXPECT_CALL(observer(), OnStartTracking(contents1(), LoadingState::UNLOADED));
  tracker().StartTracking(contents1());
  if (use_non_ui_tabs) {
    EXPECT_TAB_COUNTS(1, 1, 0, 0);
    EXPECT_UI_TAB_COUNTS(0, 0, 0, 0);
  } else {
    EXPECT_TAB_AND_UI_TAB_COUNTS(1, 1, 0, 0);
  }
  testing::Mock::VerifyAndClearExpectations(&observer());

  EXPECT_CALL(observer(), OnStartTracking(contents2(), LoadingState::LOADING));
  tracker().StartTracking(contents2());
  if (use_non_ui_tabs) {
    EXPECT_TAB_COUNTS(2, 1, 1, 0);
    EXPECT_UI_TAB_COUNTS(0, 0, 0, 0);
  } else {
    EXPECT_TAB_AND_UI_TAB_COUNTS(2, 1, 1, 0);
  }
  testing::Mock::VerifyAndClearExpectations(&observer());

  EXPECT_CALL(observer(), OnStartTracking(contents3(), LoadingState::LOADED));
  tracker().StartTracking(contents3());
  if (use_non_ui_tabs) {
    EXPECT_TAB_COUNTS(3, 1, 1, 1);
    EXPECT_UI_TAB_COUNTS(0, 0, 0, 0);
  } else {
    EXPECT_TAB_AND_UI_TAB_COUNTS(3, 1, 1, 1);
  }
  testing::Mock::VerifyAndClearExpectations(&observer());

  // Start observers for the contents.
  TestWebContentsObserver observer1(contents1(), &tracker());
  TestWebContentsObserver observer2(contents2(), &tracker());
  TestWebContentsObserver observer3(contents3(), &tracker());

  // Now test all of the possible state transitions.

  // Finish the loading for contents2.
  EXPECT_CALL(observer(),
              OnLoadingStateChange(contents2(), LoadingState::LOADING,
                                   LoadingState::LOADED));
  navigation_tab_2->StopLoading();
  // The state transition should only occur *after* the PAI signal.
  if (use_non_ui_tabs) {
    EXPECT_TAB_COUNTS(3, 1, 1, 1);
    EXPECT_UI_TAB_COUNTS(0, 0, 0, 0);
  } else {
    EXPECT_TAB_AND_UI_TAB_COUNTS(3, 1, 1, 1);
  }
  tracker().OnPageStoppedLoading(contents2());

  if (use_non_ui_tabs) {
    EXPECT_TAB_COUNTS(3, 1, 0, 2);
    EXPECT_UI_TAB_COUNTS(0, 0, 0, 0);
  } else {
    EXPECT_TAB_AND_UI_TAB_COUNTS(3, 1, 0, 2);
  }
  testing::Mock::VerifyAndClearExpectations(&observer());

  // Start the loading for contents1.
  EXPECT_CALL(observer(),
              OnLoadingStateChange(contents1(), LoadingState::UNLOADED,
                                   LoadingState::LOADING));
  auto navigation_tab_1 =
      NavigateAndKeepLoading(contents1(), GURL("http://baz.com"));
  if (use_non_ui_tabs) {
    EXPECT_TAB_COUNTS(3, 0, 1, 2);
    EXPECT_UI_TAB_COUNTS(0, 0, 0, 0);
  } else {
    EXPECT_TAB_AND_UI_TAB_COUNTS(3, 0, 1, 2);
  }
  testing::Mock::VerifyAndClearExpectations(&observer());

  // Crash the render process corresponding to the main frame of a tab. This
  // should cause the tab to transition to the UNLOADED state.
  EXPECT_CALL(observer(),
              OnLoadingStateChange(contents1(), LoadingState::LOADING,
                                   LoadingState::UNLOADED));
  content::MockRenderProcessHost* rph =
      static_cast<content::MockRenderProcessHost*>(
          contents1()->GetPrimaryMainFrame()->GetProcess());
  rph->SimulateCrash();
  if (use_non_ui_tabs) {
    EXPECT_TAB_COUNTS(3, 1, 0, 2);
    EXPECT_UI_TAB_COUNTS(0, 0, 0, 0);
  } else {
    EXPECT_TAB_AND_UI_TAB_COUNTS(3, 1, 0, 2);
  }
  testing::Mock::VerifyAndClearExpectations(&observer());
}

TEST_F(TabLoadTrackerTest, StateTransitions) {
  StateTransitionsTest(false /* use_non_ui_tabs */);
}

TEST_F(TabLoadTrackerTest, StateTransitionsNonUiTabs) {
  StateTransitionsTest(true /* use_non_ui_tabs */);
}

TEST_F(TabLoadTrackerTest, NoStatePrefetchContentsDoesNotChangeUiTabCounts) {
  NavigateAndKeepLoading(contents1(), GURL("http://baz.com"));

  // Add the contents to the tracker.
  EXPECT_CALL(observer(), OnStartTracking(contents1(), LoadingState::LOADING));
  tracker().StartTracking(contents1());
  EXPECT_TAB_AND_UI_TAB_COUNTS(1, 0, 1, 0);
  testing::Mock::VerifyAndClearExpectations(&observer());

  EXPECT_CALL(observer(), OnStartTracking(contents2(), LoadingState::UNLOADED));
  tracker().StartTracking(contents2());
  EXPECT_TAB_AND_UI_TAB_COUNTS(2, 1, 1, 0);
  testing::Mock::VerifyAndClearExpectations(&observer());

  // Start observers for the contents.
  TestWebContentsObserver observer1(contents1(), &tracker());
  TestWebContentsObserver observer2(contents2(), &tracker());

  // Prefetch some contents.
  prerender::NoStatePrefetchManager* no_state_prefetch_manager =
      prerender::NoStatePrefetchManagerFactory::GetForBrowserContext(profile());
  GURL url("http://www.example.com");
  const gfx::Size kSize(640, 480);
  std::unique_ptr<prerender::NoStatePrefetchHandle> no_state_prefetch_handle(
      no_state_prefetch_manager->StartPrefetchingFromOmnibox(
          url, contents1()->GetController().GetDefaultSessionStorageNamespace(),
          kSize, nullptr));
  EXPECT_NE(nullptr, no_state_prefetch_handle);
  const std::vector<content::WebContents*> contentses =
      no_state_prefetch_manager->GetAllNoStatePrefetchingContentsForTesting();
  ASSERT_EQ(1U, contentses.size());

  // Prefetching should not change the UI tab counts, but should increase
  // overall tab count. Note, contentses[0] is UNLOADED since it is not a test
  // web contents and therefore hasn't started receiving data.
  TestWebContentsObserver prefetch_observer(contentses[0], &tracker());
  EXPECT_CALL(observer(),
              OnStartTracking(contentses[0], LoadingState::UNLOADED));
  tracker().StartTracking(contentses[0]);
  EXPECT_TAB_COUNTS(3, 2, 1, 0);
  EXPECT_UI_TAB_COUNTS(2, 1, 1, 0);
  testing::Mock::VerifyAndClearExpectations(&observer());

  no_state_prefetch_manager->CancelAllPrerenders();
}

TEST_F(TabLoadTrackerTest, SwapInUiTabContents) {
  NavigateAndKeepLoading(contents1(), GURL("http://baz.com"));

  // Add the contents to the tracker.
  EXPECT_CALL(observer(), OnStartTracking(contents1(), LoadingState::LOADING));
  tracker().StartTracking(contents1());
  EXPECT_TAB_AND_UI_TAB_COUNTS(1, 0, 1, 0);
  testing::Mock::VerifyAndClearExpectations(&observer());

  EXPECT_CALL(observer(), OnStartTracking(contents2(), LoadingState::UNLOADED));
  tracker().StartTracking(contents2());
  EXPECT_TAB_AND_UI_TAB_COUNTS(2, 1, 1, 0);
  testing::Mock::VerifyAndClearExpectations(&observer());

  // Start observers for the contents.
  TestWebContentsObserver observer1(contents1(), &tracker());
  TestWebContentsObserver observer2(contents2(), &tracker());

  // Simulate non-ui tab contents running in the background and getting swapped
  // in. Non-ui tabs should not change the ui tab counts, but should change the
  // overall tab counts.
  std::unique_ptr<content::WebContents> non_ui_tab_contents =
      CreateTestWebContents();
  EXPECT_CALL(observer(), OnStartTracking(non_ui_tab_contents.get(),
                                          LoadingState::UNLOADED));
  tracker().SetAllTabsAreNonUiTabs(true);
  tracker().StartTracking(non_ui_tab_contents.get());
  EXPECT_TAB_COUNTS(3, 2, 1, 0);
  EXPECT_UI_TAB_COUNTS(2, 1, 1, 0);
  testing::Mock::VerifyAndClearExpectations(&observer());
  // Swap in the prerender contents and simulate resulting tab strip swap.
  // |non_ui_tab_contents| is already being tracked. The UI tab count should
  // remain stable through the swap.
  EXPECT_CALL(observer(), OnStopTracking(contents1(), LoadingState::LOADING));
  tracker().SetAllTabsAreNonUiTabs(false);
  tracker().SwapTabContents(contents1(), non_ui_tab_contents.get());
  // After swap, but before we stop tracking the swapped-out contents. The UI
  // tab counts should be in the end-state, but the total tab counts will be in
  // the pre-swap state while the swapped-out contents is still being tracked.
  EXPECT_TAB_COUNTS(3, 2, 1, 0);
  EXPECT_UI_TAB_COUNTS(2, 2, 0, 0);
  tracker().StopTracking(contents1());
  EXPECT_TAB_AND_UI_TAB_COUNTS(2, 2, 0, 0);
  testing::Mock::VerifyAndClearExpectations(&observer());
}

TEST_F(TabLoadTrackerTest, SwapInUntrackedContents) {
  NavigateAndKeepLoading(contents1(), GURL("http://baz.com"));

  // Add the contents to the tracker.
  EXPECT_CALL(observer(), OnStartTracking(contents1(), LoadingState::LOADING));
  tracker().StartTracking(contents1());
  EXPECT_TAB_AND_UI_TAB_COUNTS(1, 0, 1, 0);
  testing::Mock::VerifyAndClearExpectations(&observer());

  EXPECT_CALL(observer(), OnStartTracking(contents2(), LoadingState::UNLOADED));
  tracker().StartTracking(contents2());
  EXPECT_TAB_AND_UI_TAB_COUNTS(2, 1, 1, 0);
  testing::Mock::VerifyAndClearExpectations(&observer());

  // Create an untracked web contents in the UNLOADED state, and swap it with
  // the contents in the LOADING state. Since |untracked_contents| has no tab
  // helper attached, swapping it in shouldn't changed the tab count.
  std::unique_ptr<content::WebContents> untracked_contents =
      CreateTestWebContents();
  tracker().SwapTabContents(contents1(), untracked_contents.get());
  // The total counts will remain stable since swapping out doesn't cause any
  // web contents to stop being tracking. However, the swapped-out contents are
  // no longer included in UI tab counts, and the swapped-in contents won't be
  // until it is tracked.
  EXPECT_TAB_COUNTS(2, 1, 1, 0);
  EXPECT_UI_TAB_COUNTS(1, 1, 0, 0);

  // Simulate swap in tab strip, which would cause |untracked_contents| to be
  // tracked and the tab counts to change.
  EXPECT_CALL(observer(), OnStopTracking(contents1(), LoadingState::LOADING));
  EXPECT_CALL(observer(), OnStartTracking(untracked_contents.get(),
                                          LoadingState::UNLOADED));
  tracker().StopTracking(contents1());
  tracker().StartTracking(untracked_contents.get());
  EXPECT_TAB_AND_UI_TAB_COUNTS(2, 2, 0, 0);
}

}  // namespace resource_coordinator
