// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/tab_load_tracker.h"

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/process/kill.h"
#include "base/strings/utf_string_conversions.h"
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

  TestTabLoadTracker() = default;
  virtual ~TestTabLoadTracker() = default;

  // Some accessors for TabLoadTracker internals.
  const TabMap& tabs() const { return tabs_; }
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

  void StateTransitionsTest();

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

void TabLoadTrackerTest::StateTransitionsTest() {
  // Set up the contents in UNLOADED, LOADING and LOADED states. This tests
  // each possible "entry" state.
  auto navigation_tab_2 =
      NavigateAndKeepLoading(contents2(), GURL("http://foo.com"));
  NavigateAndFinishLoading(contents3(), GURL("http://bar.com"));

  // Add the contents to the tracker.
  EXPECT_CALL(observer(), OnStartTracking(contents1(), LoadingState::UNLOADED));
  tracker().StartTracking(contents1());
  EXPECT_TAB_COUNTS(1, 1, 0, 0);
  testing::Mock::VerifyAndClearExpectations(&observer());

  EXPECT_CALL(observer(), OnStartTracking(contents2(), LoadingState::LOADING));
  tracker().StartTracking(contents2());
  EXPECT_TAB_COUNTS(2, 1, 1, 0);
  testing::Mock::VerifyAndClearExpectations(&observer());

  EXPECT_CALL(observer(), OnStartTracking(contents3(), LoadingState::LOADED));
  tracker().StartTracking(contents3());
  EXPECT_TAB_COUNTS(3, 1, 1, 1);
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
  EXPECT_TAB_COUNTS(3, 1, 1, 1);
  tracker().OnPageStoppedLoading(contents2());
  EXPECT_TAB_COUNTS(3, 1, 0, 2);
  testing::Mock::VerifyAndClearExpectations(&observer());

  // Start the loading for contents1.
  EXPECT_CALL(observer(),
              OnLoadingStateChange(contents1(), LoadingState::UNLOADED,
                                   LoadingState::LOADING));
  auto navigation_tab_1 =
      NavigateAndKeepLoading(contents1(), GURL("http://baz.com"));
  EXPECT_TAB_COUNTS(3, 0, 1, 2);
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
  EXPECT_TAB_COUNTS(3, 1, 0, 2);
  testing::Mock::VerifyAndClearExpectations(&observer());
}

TEST_F(TabLoadTrackerTest, StateTransitions) {
  StateTransitionsTest();
}

}  // namespace resource_coordinator
