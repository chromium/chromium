// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/tab_stats/tab_stats_tracker.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/android_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/web_contents_tester.h"
#include "media/base/media_switches.h"
#include "media/base/test_data_util.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/browser/ui/android/tab_model/tab_model_test_helper.h"
#include "chrome/test/base/android/android_browser_test.h"
#else
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_ui_types.h"
#endif

namespace metrics {

namespace {

class TestTabStatsObserver : public TabStatsObserver {
 public:
  // Functions used to update the window/tab count.
  void OnWindowAdded() override { ++window_count_; }
  void OnWindowRemoved() override {
    EXPECT_GT(window_count_, 0U);
    --window_count_;
  }
  void OnTabAdded(content::WebContents* web_contents) override { ++tab_count_; }
  void OnTabRemoved(content::WebContents* web_contents) override {
    EXPECT_GT(tab_count_, 0U);
    --tab_count_;
  }

  void OnTabInteraction(content::WebContents* web_contents) override {
    ++interaction_count_;
  }

  size_t tab_count() { return tab_count_; }

  size_t window_count() { return window_count_; }

  size_t interaction_count() { return interaction_count_; }

 private:
  size_t tab_count_ = 0;
  size_t window_count_ = 0;
  size_t interaction_count_ = 0;
};

using TabsStats = TabStatsDataStore::TabsStats;
using TabStripInterface = TabStatsTracker::TabStripInterface;
using ::testing::_;
using ::testing::AllOf;
using ::testing::Ne;

void EnsureTabStatsMatchExpectations(
    const TabsStats& expected,
    const TabsStats& actual,
    const base::Location& location = base::Location::Current()) {
  SCOPED_TRACE(location.ToString());
  EXPECT_EQ(expected.total_tab_count, actual.total_tab_count);
  EXPECT_EQ(expected.total_tab_count_max, actual.total_tab_count_max);
  EXPECT_EQ(expected.max_tab_per_window, actual.max_tab_per_window);
  EXPECT_EQ(expected.window_count, actual.window_count);
  EXPECT_EQ(expected.window_count_max, actual.window_count_max);
}

#if BUILDFLAG(IS_ANDROID)
using PlatformBrowserTest = AndroidBrowserTest;
#else
using PlatformBrowserTest = InProcessBrowserTest;
#endif

}  // namespace

class TabStatsTrackerBrowserTest : public PlatformBrowserTest {
 public:
  struct HistogramStats {
    std::string name;
    std::map<int, int> buckets;
  };

  struct DuplicateHistogramStats {
    HistogramStats count_single_window;
    HistogramStats percentage_single_window;
    HistogramStats count_multi_window;
    HistogramStats percentage_multi_window;
  };

  TabStatsTrackerBrowserTest() = default;

  TabStatsTrackerBrowserTest(const TabStatsTrackerBrowserTest&) = delete;
  TabStatsTrackerBrowserTest& operator=(const TabStatsTrackerBrowserTest&) =
      delete;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(
        switches::kAutoplayPolicy,
        switches::autoplay::kNoUserGestureRequiredPolicy);
  }

  void SetUpOnMainThread() override {
    tab_stats_tracker_ = TabStatsTracker::GetInstance();
    ASSERT_TRUE(tab_stats_tracker_ != nullptr);

#if BUILDFLAG(IS_ANDROID)
    ASSERT_FALSE(TabModelList::models().empty());
    tab_strip_ = std::make_unique<TabStripInterface>(
        TabModelList::models().front().get());

    // The initial tab of the main TabModel will be in an inconsistent state,
    // since its WebContents is created and navigates to an initial URL
    // asynchronously. Wait for it to finish loading.
    // TODO(crbug.com/412634171): Occasionally tests start with more than 1 tab
    // in the TabModel. Find out why, and if it's a cause of flakes.
    ASSERT_GE(tab_strip_->tab_model()->GetTabCount(), 1);
    TabAndroidLoadedWaiter waiter(tab_strip_->tab_model()->GetTabAt(0));
    ASSERT_TRUE(waiter.Wait());
#else
    ASSERT_TRUE(browser());
    tab_strip_ = std::make_unique<TabStripInterface>(browser());
#endif
  }

  void TearDownOnMainThread() override {
    tab_strip_.reset();
    tab_stats_tracker_ = nullptr;

#if BUILDFLAG(IS_ANDROID)
    extra_tab_models_.clear();
#endif
  }

  TabStripInterface& tab_strip() { return *tab_strip_; }

  content::WebContents* GetWebContents() {
    return tab_strip().GetActiveWebContents();
  }

  // Methods to manipulate Browser + TabStripModel (on desktop) or TabModel (on
  // Android).

#if BUILDFLAG(IS_ANDROID)

  void NavigateNewTabToUrl(content::WebContents* contents, const GURL& url) {
    // Navigate to `url` as a render-initiated navigation, so that it isn't
    // considered a user interaction.
    content::NavigationController::LoadURLParams load_params(url);
    load_params.initiator_origin = url::Origin();
    load_params.is_renderer_initiated = true;
    content::NavigateToURLBlockUntilNavigationsComplete(contents, load_params,
                                                        1);
  }

  bool AddTabToTabStrip(TabStripInterface& tab_strip,
                        const GURL& url = GURL("about:blank")) {
    content::WebContents* active_contents = tab_strip.GetActiveWebContents();
    if (!active_contents) {
      ADD_FAILURE() << "No active WebContents";
      return false;
    }

    // Create the WebContents hidden so that there's no visibility notification
    // until it's added to the tab strip.
    content::WebContents::CreateParams create_params(tab_strip.GetProfile());
    create_params.initially_hidden = true;
    content::WebContents* new_contents =
        content::WebContents::Create(create_params).release();

    // CreateTab works with both OwningTestTabModel and the initial tab strip,
    // which is a production TabModel.
    tab_strip.tab_model()->CreateTab(
        TabAndroid::FromWebContents(active_contents), new_contents,
        /*select=*/true);

    NavigateNewTabToUrl(new_contents, url);
    return true;
  }

  std::unique_ptr<TabStripInterface> CreateTabStripInProfile(Profile* profile) {
    extra_tab_models_.push_back(std::make_unique<OwningTestTabModel>(profile));
    TabAndroid* new_tab = extra_tab_models_.back()->AddEmptyTab(0);
    // Navigate the new tab to "about:blank" so that it has the same behaviour
    // as CreateBrowser on desktop.
    NavigateNewTabToUrl(new_tab->web_contents(), GURL("about:blank"));
    return std::make_unique<TabStripInterface>(extra_tab_models_.back().get());
  }

  void CloseTabStrip(
      std::unique_ptr<TabStripInterface> tab_strip,
      const base::Location& location = base::Location::Current()) {
    SCOPED_TRACE(location.ToString());
    TabModel* tab_model = tab_strip->tab_model();
    for (auto it = extra_tab_models_.begin(); it != extra_tab_models_.end();
         ++it) {
      if (it->get() == tab_model) {
        // Destroy `tab_strip` before `tab_model` to avoid a dangling raw_ref.
        tab_strip.reset();
        extra_tab_models_.erase(it);
        return;
      }
    }
    FAIL() << "Can't close a tab strip the test didn't open";
  }

#else  // !BUILDFLAG(IS_ANDROID)

  bool AddTabToTabStrip(TabStripInterface& tab_strip,
                        const GURL& url = GURL("about:blank")) {
    return AddTabAtIndexToBrowser(tab_strip.browser_window_interface(), 1, url,
                                  ui::PAGE_TRANSITION_TYPED);
  }

  std::unique_ptr<TabStripInterface> CreateTabStripInProfile(Profile* profile) {
    return std::make_unique<TabStripInterface>(CreateBrowser(profile));
  }

  void CloseTabStrip(std::unique_ptr<TabStripInterface> tab_strip) {
    BrowserWindowInterface* browser = tab_strip->browser_window_interface();
    // Destroy `tab_strip` before `browser` to avoid a dangling raw_ref.
    tab_strip.reset();
    CloseBrowserSynchronously(browser);
  }

  void CloseMainTabStrip() {
    CloseTabStrip(std::move(tab_strip_));
    EXPECT_FALSE(tab_strip_);
  }

#endif  // !BUILDFLAG(IS_ANDROID)

  void EnsureTabDuplicateHistogramsMatchExpectations(
      DuplicateHistogramStats expected,
      const base::Location& location = base::Location::Current()) {
    SCOPED_TRACE(location.ToString());
    for (auto const& bucket : expected.count_multi_window.buckets) {
      histogram_tester_.ExpectBucketCount(expected.count_multi_window.name,
                                          bucket.first, bucket.second);
    }
    for (auto const& bucket : expected.percentage_multi_window.buckets) {
      histogram_tester_.ExpectBucketCount(expected.percentage_multi_window.name,
                                          bucket.first, bucket.second);
    }
    for (auto const& bucket : expected.count_single_window.buckets) {
      histogram_tester_.ExpectBucketCount(expected.count_single_window.name,
                                          bucket.first, bucket.second);
    }
    for (auto const& bucket : expected.percentage_single_window.buckets) {
      histogram_tester_.ExpectBucketCount(
          expected.percentage_single_window.name, bucket.first, bucket.second);
    }
  }

 protected:
  // Used to make sure that the metrics are reported properly.
  base::HistogramTester histogram_tester_;

  raw_ptr<TabStatsTracker> tab_stats_tracker_{nullptr};
  std::vector<std::unique_ptr<TestTabStatsObserver>> test_tab_stats_observers_;

  std::unique_ptr<TabStripInterface> tab_strip_;

#if BUILDFLAG(IS_ANDROID)
  std::vector<std::unique_ptr<OwningTestTabModel>> extra_tab_models_;
#endif
};

IN_PROC_BROWSER_TEST_F(TabStatsTrackerBrowserTest,
                       TabsAndWindowsAreCountedAccurately) {
  // Assert that the |TabStatsTracker| instance is initialized during the
  // creation of the main browser.
  ASSERT_TRUE(tab_stats_tracker_ != nullptr);

  TabsStats expected_stats = {};

  // There should be only one window with one tab at startup.
  expected_stats.total_tab_count = 1;
  expected_stats.total_tab_count_max = 1;
  expected_stats.max_tab_per_window = 1;
  expected_stats.window_count = 1;
  expected_stats.window_count_max = 1;

  DuplicateHistogramStats expected_histograms = {};

  expected_histograms.count_multi_window.name =
      TabStatsTracker::UmaStatsReportingDelegate::
          kTabDuplicateCountAllProfileWindowsHistogramName;
  expected_histograms.percentage_multi_window.name =
      TabStatsTracker::UmaStatsReportingDelegate::
          kTabDuplicatePercentageAllProfileWindowsHistogramName;
  expected_histograms.count_single_window.name = TabStatsTracker::
      UmaStatsReportingDelegate::kTabDuplicateCountSingleWindowHistogramName;
  expected_histograms.percentage_single_window.name =
      TabStatsTracker::UmaStatsReportingDelegate::
          kTabDuplicatePercentageSingleWindowHistogramName;
  expected_histograms.count_multi_window.buckets[0] = 1;
  expected_histograms.percentage_multi_window.buckets[0] = 1;
  expected_histograms.count_single_window.buckets[0] = 1;
  expected_histograms.percentage_single_window.buckets[0] = 1;

  EnsureTabStatsMatchExpectations(expected_stats,
                                  tab_stats_tracker_->tab_stats());
  tab_stats_tracker_->reporting_delegate_for_testing()
      ->ReportTabDuplicateMetrics(false);
  EnsureTabDuplicateHistogramsMatchExpectations(expected_histograms);

  // Add a tab and make sure that the counters get updated.
  ASSERT_TRUE(AddTabToTabStrip(tab_strip()));
  ++expected_stats.total_tab_count;
  ++expected_stats.total_tab_count_max;
  ++expected_stats.max_tab_per_window;
  expected_histograms.count_multi_window.buckets[1] = 1;
  expected_histograms.percentage_multi_window.buckets[50] = 1;
  expected_histograms.count_single_window.buckets[1] = 1;
  expected_histograms.percentage_single_window.buckets[50] = 1;
  EnsureTabStatsMatchExpectations(expected_stats,
                                  tab_stats_tracker_->tab_stats());
  tab_stats_tracker_->reporting_delegate_for_testing()
      ->ReportTabDuplicateMetrics(false);
  EnsureTabDuplicateHistogramsMatchExpectations(expected_histograms);

  tab_strip().CloseTabAtForTesting(1);
  --expected_stats.total_tab_count;
  ++expected_histograms.count_multi_window.buckets[0];
  ++expected_histograms.percentage_multi_window.buckets[0];
  ++expected_histograms.count_single_window.buckets[0];
  ++expected_histograms.percentage_single_window.buckets[0];
  EnsureTabStatsMatchExpectations(expected_stats,
                                  tab_stats_tracker_->tab_stats());
  tab_stats_tracker_->reporting_delegate_for_testing()
      ->ReportTabDuplicateMetrics(false);
  EnsureTabDuplicateHistogramsMatchExpectations(expected_histograms);

  std::unique_ptr<TabStripInterface> new_tab_strip =
      CreateTabStripInProfile(tab_strip().GetProfile());
  ASSERT_TRUE(new_tab_strip);
  ++expected_stats.total_tab_count;
  ++expected_stats.window_count;
  ++expected_stats.window_count_max;
  ++expected_histograms.count_multi_window.buckets[1];
  ++expected_histograms.percentage_multi_window.buckets[50];
  expected_histograms.count_single_window.buckets[0] += 2;
  expected_histograms.percentage_single_window.buckets[0] += 2;
  EnsureTabStatsMatchExpectations(expected_stats,
                                  tab_stats_tracker_->tab_stats());
  tab_stats_tracker_->reporting_delegate_for_testing()
      ->ReportTabDuplicateMetrics(false);
  EnsureTabDuplicateHistogramsMatchExpectations(expected_histograms);

  ASSERT_TRUE(AddTabToTabStrip(*new_tab_strip));
  ++expected_stats.total_tab_count;
  ++expected_stats.total_tab_count_max;
  expected_histograms.count_multi_window.buckets[2] = 1;
  expected_histograms.percentage_multi_window.buckets[66] = 1;
  ++expected_histograms.count_single_window.buckets[1];
  ++expected_histograms.count_single_window.buckets[0];
  ++expected_histograms.percentage_single_window.buckets[0];
  ++expected_histograms.percentage_single_window.buckets[50];
  EnsureTabStatsMatchExpectations(expected_stats,
                                  tab_stats_tracker_->tab_stats());
  tab_stats_tracker_->reporting_delegate_for_testing()
      ->ReportTabDuplicateMetrics(false);
  EnsureTabDuplicateHistogramsMatchExpectations(expected_histograms);

  CloseTabStrip(std::move(new_tab_strip));
  expected_stats.total_tab_count = 1;
  expected_stats.window_count = 1;
  ++expected_histograms.count_multi_window.buckets[0];
  ++expected_histograms.percentage_multi_window.buckets[0];
  ++expected_histograms.count_single_window.buckets[0];
  ++expected_histograms.percentage_single_window.buckets[0];
  EnsureTabStatsMatchExpectations(expected_stats,
                                  tab_stats_tracker_->tab_stats());
  tab_stats_tracker_->reporting_delegate_for_testing()
      ->ReportTabDuplicateMetrics(false);
  EnsureTabDuplicateHistogramsMatchExpectations(expected_histograms);
}

IN_PROC_BROWSER_TEST_F(TabStatsTrackerBrowserTest,
                       AdditionalTabStatsObserverGetsInitiliazed) {
  // Assert that the |TabStatsTracker| instance is initialized during the
  // creation of the main browser.
  ASSERT_TRUE(tab_stats_tracker_ != nullptr);

  TabsStats expected_stats = {};

  // There should be only one window with one tab at startup.
  expected_stats.total_tab_count = 1;
  expected_stats.total_tab_count_max = 1;
  expected_stats.max_tab_per_window = 1;
  expected_stats.window_count = 1;
  expected_stats.window_count_max = 1;

  EnsureTabStatsMatchExpectations(expected_stats,
                                  tab_stats_tracker_->tab_stats());

  test_tab_stats_observers_.push_back(std::make_unique<TestTabStatsObserver>());
  TestTabStatsObserver* first_observer = test_tab_stats_observers_.back().get();
  tab_stats_tracker_->AddObserverAndSetInitialState(first_observer);

  // Observer is initialized properly.
  EXPECT_EQ(first_observer->tab_count(), expected_stats.total_tab_count);
  EXPECT_EQ(first_observer->window_count(), expected_stats.window_count);

  // Add some tabs and windows to increase the counts.
  std::unique_ptr<TabStripInterface> new_tab_strip =
      CreateTabStripInProfile(tab_strip().GetProfile());
  ASSERT_TRUE(new_tab_strip);
  ++expected_stats.total_tab_count;
  ++expected_stats.window_count;

  ASSERT_TRUE(AddTabToTabStrip(*new_tab_strip));
  ++expected_stats.total_tab_count;

  test_tab_stats_observers_.push_back(std::make_unique<TestTabStatsObserver>());
  TestTabStatsObserver* second_observer =
      test_tab_stats_observers_.back().get();
  tab_stats_tracker_->AddObserverAndSetInitialState(second_observer);

  // Observer is initialized properly.
  EXPECT_EQ(second_observer->tab_count(), expected_stats.total_tab_count);
  EXPECT_EQ(second_observer->window_count(), expected_stats.window_count);

  tab_stats_tracker_->RemoveObserver(first_observer);
  tab_stats_tracker_->RemoveObserver(second_observer);
}

namespace {

class LenientMockTabStatsObserver : public TabStatsObserver {
 public:
  LenientMockTabStatsObserver() = default;
  ~LenientMockTabStatsObserver() override = default;
  LenientMockTabStatsObserver(const LenientMockTabStatsObserver& other) =
      delete;
  LenientMockTabStatsObserver& operator=(const LenientMockTabStatsObserver&) =
      delete;

  MOCK_METHOD0(OnWindowAdded, void());
  MOCK_METHOD0(OnWindowRemoved, void());
  MOCK_METHOD1(OnTabAdded, void(content::WebContents*));
  MOCK_METHOD1(OnTabRemoved, void(content::WebContents*));
  MOCK_METHOD2(OnTabReplaced,
               void(content::WebContents*, content::WebContents*));
  MOCK_METHOD1(OnPrimaryMainFrameNavigationCommitted,
               void(content::WebContents*));
  MOCK_METHOD1(OnTabInteraction, void(content::WebContents*));
  MOCK_METHOD1(OnTabIsAudibleChanged, void(content::WebContents*));
  MOCK_METHOD1(OnTabVisibilityChanged, void(content::WebContents*));
  MOCK_METHOD2(OnMediaEffectivelyFullscreenChanged,
               void(content::WebContents*, bool));
};
using MockTabStatsObserver = testing::NiceMock<LenientMockTabStatsObserver>;

}  // namespace

// TODO(crbug.com/40752198): Fix the flakiness on MacOS and re-enable the test.
#if BUILDFLAG(IS_MAC)
#define MAYBE_TabStatsObserverBasics DISABLED_TabStatsObserverBasics
#else
#define MAYBE_TabStatsObserverBasics TabStatsObserverBasics
#endif
IN_PROC_BROWSER_TEST_F(TabStatsTrackerBrowserTest,
                       MAYBE_TabStatsObserverBasics) {
  MockTabStatsObserver mock_observer;
  TestTabStatsObserver count_observer;
  tab_stats_tracker_->AddObserverAndSetInitialState(&count_observer);

  auto* window1_tab1 = tab_strip().GetWebContentsAt(0);
  ASSERT_TRUE(window1_tab1);
  EXPECT_EQ(content::Visibility::VISIBLE, window1_tab1->GetVisibility());

  // The browser starts with one window and one visible tab, the observer will
  // be notified immediately about those.
  EXPECT_CALL(mock_observer, OnWindowAdded());
  EXPECT_CALL(mock_observer, OnTabAdded(window1_tab1));
  tab_stats_tracker_->AddObserverAndSetInitialState(&mock_observer);
  ::testing::Mock::VerifyAndClear(&mock_observer);

  // Mark the tab as hidden.
  EXPECT_CALL(mock_observer, OnTabVisibilityChanged(window1_tab1));
  window1_tab1->WasHidden();
  EXPECT_EQ(content::Visibility::HIDDEN, window1_tab1->GetVisibility());
  ::testing::Mock::VerifyAndClear(&mock_observer);

  // Make it visible again.
  EXPECT_CALL(mock_observer, OnTabVisibilityChanged(window1_tab1));
  window1_tab1->WasShown();
  EXPECT_EQ(content::Visibility::VISIBLE, window1_tab1->GetVisibility());
  ::testing::Mock::VerifyAndClear(&mock_observer);

  // Create a second browser window. This will cause one visible tab to be
  // created and its main frame will do a navigation.
  // The pointer to the new tab isn't available yet when setting expectations,
  // so match any pointer that's not a tab that already exists.
  auto window2_tab1_matcher = Ne(window1_tab1);
  EXPECT_CALL(mock_observer, OnWindowAdded());
  EXPECT_CALL(mock_observer, OnTabAdded(window2_tab1_matcher));
  EXPECT_CALL(mock_observer,
              OnPrimaryMainFrameNavigationCommitted(window2_tab1_matcher));
  EXPECT_CALL(mock_observer, OnTabVisibilityChanged(window2_tab1_matcher));

  std::unique_ptr<TabStripInterface> window2 =
      CreateTabStripInProfile(tab_strip().GetProfile());
  auto* window2_tab1 = window2->GetWebContentsAt(0);
  ASSERT_TRUE(window2_tab1);
  EXPECT_EQ(content::Visibility::VISIBLE, window1_tab1->GetVisibility());
  EXPECT_EQ(content::Visibility::VISIBLE, window2_tab1->GetVisibility());
  ::testing::Mock::VerifyAndClear(&mock_observer);

#if BUILDFLAG(IS_ANDROID)
  // Can't move the windows around from within the test, so hide the first
  // tab again to prevent occlusion. This doesn't work on all platforms because
  // the desktop occlusion tracker may un-hide the tab.
  EXPECT_CALL(mock_observer, OnTabVisibilityChanged(window1_tab1));
  window1_tab1->WasHidden();
  auto expected_window1_tab1_visibility = content::Visibility::HIDDEN;
#else
  // Make sure that the 2 windows don't overlap to avoid some unexpected
  // visibility change events because one tab occludes the other.
  // This resizes the two windows so they're right next to each other.
  const gfx::NativeWindow window = browser()->window()->GetNativeWindow();
  gfx::Rect work_area =
      display::Screen::Get()->GetDisplayNearestWindow(window).work_area();
  const gfx::Size size(work_area.width() / 3, work_area.height() / 2);
  gfx::Rect browser_rect(work_area.origin(), size);
  browser()->window()->SetBounds(browser_rect);
  browser_rect.set_x(browser_rect.right());
  window2->browser_window_interface()->GetWindow()->SetBounds(browser_rect);
  auto expected_window1_tab1_visibility = content::Visibility::VISIBLE;
#endif

  // Adding a tab to the second window will cause its previous frame to become
  // hidden.
  // The pointer to the new tab isn't available yet when setting expectations,
  // so match any pointer that's not a tab that already exists.
  auto window2_tab2_matcher = AllOf(Ne(window1_tab1), Ne(window2_tab1));
  EXPECT_CALL(mock_observer, OnTabAdded(window2_tab2_matcher));
  EXPECT_CALL(mock_observer,
              OnPrimaryMainFrameNavigationCommitted(window2_tab2_matcher));
  EXPECT_CALL(mock_observer, OnTabVisibilityChanged(window2_tab1));
#if BUILDFLAG(IS_ANDROID)
  // On Android a visibility notification will also be sent for the new tab. On
  // desktop it starts already visible.
  EXPECT_CALL(mock_observer, OnTabVisibilityChanged(window2_tab2_matcher));
#endif

  ASSERT_TRUE(AddTabToTabStrip(*window2));
  auto* window2_tab2 = window2->GetWebContentsAt(1);
  ASSERT_TRUE(window2_tab2);
  EXPECT_EQ(expected_window1_tab1_visibility, window1_tab1->GetVisibility());
  EXPECT_EQ(content::Visibility::HIDDEN, window2_tab1->GetVisibility());
  EXPECT_EQ(content::Visibility::VISIBLE, window2_tab2->GetVisibility());
  ::testing::Mock::VerifyAndClear(&mock_observer);

  // Make sure that the visibility change events are properly forwarded.
  EXPECT_CALL(mock_observer, OnTabVisibilityChanged(window2_tab2));
  EXPECT_CALL(mock_observer, OnTabVisibilityChanged(window2_tab1));
  window2->ActivateTabAtForTesting(0);
  EXPECT_EQ(expected_window1_tab1_visibility, window1_tab1->GetVisibility());
  EXPECT_EQ(content::Visibility::VISIBLE, window2_tab1->GetVisibility());
  EXPECT_EQ(content::Visibility::HIDDEN, window2_tab2->GetVisibility());
  ::testing::Mock::VerifyAndClear(&mock_observer);

  EXPECT_CALL(mock_observer, OnTabVisibilityChanged(window2_tab1));
  EXPECT_CALL(mock_observer, OnTabRemoved(window2_tab1));
  EXPECT_CALL(mock_observer, OnTabRemoved(window2_tab2));
  EXPECT_CALL(mock_observer, OnWindowRemoved());
  CloseTabStrip(std::move(window2));
  EXPECT_EQ(expected_window1_tab1_visibility, window1_tab1->GetVisibility());
  ::testing::Mock::VerifyAndClear(&mock_observer);

#if BUILDFLAG(IS_ANDROID)
  // Can't close the main TabModel, so there will be a tab remaining.
  const size_t expected_window_count = 1;
#else
  EXPECT_CALL(mock_observer, OnTabRemoved(window1_tab1));
  EXPECT_CALL(mock_observer, OnTabVisibilityChanged(window1_tab1));
  EXPECT_CALL(mock_observer, OnWindowRemoved());
  CloseMainTabStrip();
  ::testing::Mock::VerifyAndClear(&mock_observer);
  const size_t expected_window_count = 0;
#endif

  tab_stats_tracker_->RemoveObserver(&mock_observer);
  tab_stats_tracker_->RemoveObserver(&count_observer);
  EXPECT_EQ(expected_window_count, count_observer.tab_count());
  EXPECT_EQ(expected_window_count, count_observer.window_count());
}

// TODO(crbug.com/40919431): Re-enable this test
#if BUILDFLAG(IS_MAC)
#define MAYBE_TabSwitch DISABLED_TabSwitch
#else
#define MAYBE_TabSwitch TabSwitch
#endif
IN_PROC_BROWSER_TEST_F(TabStatsTrackerBrowserTest, MAYBE_TabSwitch) {
  MockTabStatsObserver mock_observer;
  TestTabStatsObserver count_observer;
  tab_stats_tracker_->AddObserverAndSetInitialState(&count_observer);

  auto* window1_tab1 = tab_strip().GetWebContentsAt(0);
  ASSERT_TRUE(window1_tab1);
  EXPECT_EQ(content::Visibility::VISIBLE, window1_tab1->GetVisibility());

  // The browser starts with one window and one visible tab, the observer will
  // be notified immediately about those.
  EXPECT_CALL(mock_observer, OnWindowAdded());
  EXPECT_CALL(mock_observer, OnTabAdded(window1_tab1));
  tab_stats_tracker_->AddObserverAndSetInitialState(&mock_observer);
  ::testing::Mock::VerifyAndClear(&mock_observer);

  // Adding a new foreground tab will hide the original tab.
  // The pointer to the new tab isn't available yet when setting expectations,
  // so match any pointer that's not a tab that already exists.
  auto window1_tab2_matcher = Ne(window1_tab1);
  EXPECT_CALL(mock_observer, OnTabAdded(window1_tab2_matcher));
  EXPECT_CALL(mock_observer,
              OnPrimaryMainFrameNavigationCommitted(window1_tab2_matcher));
  EXPECT_CALL(mock_observer, OnTabVisibilityChanged(window1_tab1));
#if BUILDFLAG(IS_ANDROID)
  // On Android a visibility notification will also be sent for the new tab. On
  // desktop it starts already visible.
  EXPECT_CALL(mock_observer, OnTabVisibilityChanged(window1_tab2_matcher));
#endif

  ASSERT_TRUE(AddTabToTabStrip(tab_strip()));
  auto* window1_tab2 = tab_strip().GetWebContentsAt(1);
  ASSERT_TRUE(window1_tab2);
  EXPECT_EQ(content::Visibility::HIDDEN, window1_tab1->GetVisibility());
  EXPECT_EQ(content::Visibility::VISIBLE, window1_tab2->GetVisibility());
  ::testing::Mock::VerifyAndClear(&mock_observer);

  // A tab switch should cause 2 visibility change events. The "tab hidden"
  // notification should arrive before the "tab visible" one.
  {
    ::testing::InSequence s;
    EXPECT_CALL(mock_observer, OnTabVisibilityChanged(window1_tab2));
    EXPECT_CALL(mock_observer, OnTabVisibilityChanged(window1_tab1));
    tab_strip().ActivateTabAtForTesting(0);
    EXPECT_EQ(content::Visibility::VISIBLE, window1_tab1->GetVisibility());
    EXPECT_EQ(content::Visibility::HIDDEN, window1_tab2->GetVisibility());
    ::testing::Mock::VerifyAndClear(&mock_observer);
  }

  tab_stats_tracker_->RemoveObserver(&mock_observer);
  tab_stats_tracker_->RemoveObserver(&count_observer);
  EXPECT_EQ(2U, count_observer.tab_count());
  EXPECT_EQ(1U, count_observer.window_count());
}

namespace {

// Observes a WebContents and waits until it becomes audible.
// both indicate that they are audible.
class AudioStartObserver : public content::WebContentsObserver {
 public:
  explicit AudioStartObserver(content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents) {
    if (web_contents->IsCurrentlyAudible()) {
      waiter_.OnEvent();
    }
  }

  ~AudioStartObserver() override = default;

  void Wait() { EXPECT_TRUE(waiter_.Wait()); }

  // WebContentsObserver:
  void OnAudioStateChanged(bool audible) override {
    ASSERT_TRUE(audible);
    waiter_.OnEvent();
  }

 private:
  content::WaiterHelper waiter_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(TabStatsTrackerBrowserTest, AddObserverAudibleTab) {
  // Set up the embedded test server to serve the test javascript file.
  embedded_test_server()->ServeFilesFromSourceDirectory(
      media::GetTestDataPath());
  ASSERT_TRUE(embedded_test_server()->Start());

  // Open the test JS file in the only WebContents.
  auto* web_contents = tab_strip().GetWebContentsAt(0);
  ASSERT_TRUE(web_contents);
  ASSERT_FALSE(web_contents->IsCurrentlyAudible());
  ASSERT_TRUE(content::NavigateToURL(
      web_contents,
      embedded_test_server()->GetURL("/webaudio_oscillator.html")));

  // Start the audio.
  AudioStartObserver audio_start_observer(web_contents);
  EXPECT_EQ("OK", content::EvalJs(web_contents, "StartOscillator();"));
  audio_start_observer.Wait();

  // Adding an observer now should receive the OnTabIsAudibleChanged() call.
  MockTabStatsObserver mock_observer;
  EXPECT_CALL(mock_observer, OnWindowAdded());
  EXPECT_CALL(mock_observer, OnTabAdded(web_contents));
  EXPECT_CALL(mock_observer, OnTabIsAudibleChanged(web_contents));
  tab_stats_tracker_->AddObserverAndSetInitialState(&mock_observer);

  // Clean up.
  tab_stats_tracker_->RemoveObserver(&mock_observer);
}

class TabStatsTrackerPrerenderBrowserTest : public TabStatsTrackerBrowserTest {
 public:
  TabStatsTrackerPrerenderBrowserTest()
      : prerender_helper_(base::BindRepeating(
            &TabStatsTrackerPrerenderBrowserTest::GetWebContents,
            base::Unretained(this))) {}
  ~TabStatsTrackerPrerenderBrowserTest() override = default;
  TabStatsTrackerPrerenderBrowserTest(
      const TabStatsTrackerPrerenderBrowserTest&) = delete;

  TabStatsTrackerPrerenderBrowserTest& operator=(
      const TabStatsTrackerPrerenderBrowserTest&) = delete;

  void SetUp() override {
    prerender_helper_.RegisterServerRequestMonitor(embedded_test_server());
    TabStatsTrackerBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
    TabStatsTrackerBrowserTest::SetUpOnMainThread();
  }

  content::test::PrerenderTestHelper& prerender_test_helper() {
    return prerender_helper_;
  }

 private:
  content::test::PrerenderTestHelper prerender_helper_;
};

// TODO(crbug.com/412634171): On desktop Android, the prerender fails with error
// kActivationNavigationParameterMismatch. Find out why.
// TODO(crbug.com/455855986): Also starting failing on tablets.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_PrerenderingShouldNotCallOnPrimaryMainFrameNavigationCommitted \
  DISABLED_PrerenderingShouldNotCallOnPrimaryMainFrameNavigationCommitted
#else
#define MAYBE_PrerenderingShouldNotCallOnPrimaryMainFrameNavigationCommitted \
  PrerenderingShouldNotCallOnPrimaryMainFrameNavigationCommitted
#endif
IN_PROC_BROWSER_TEST_F(
    TabStatsTrackerPrerenderBrowserTest,
    MAYBE_PrerenderingShouldNotCallOnPrimaryMainFrameNavigationCommitted) {
  GURL initial_url = embedded_test_server()->GetURL("/empty.html");
  GURL prerender_url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(GetWebContents());
  ASSERT_TRUE(content::NavigateToURL(GetWebContents(), initial_url));

  std::unique_ptr<content::test::PrerenderHostObserver> host_observer;

  // OnPrimaryMainFrameNavigationCommitted() should not be called in
  // prerendering.
  {
    MockTabStatsObserver mock_observer;
    tab_stats_tracker_->AddObserverAndSetInitialState(&mock_observer);
    EXPECT_CALL(mock_observer, OnPrimaryMainFrameNavigationCommitted(_))
        .Times(0);
    content::FrameTreeNodeId host_id =
        prerender_test_helper().AddPrerender(prerender_url);
    ASSERT_TRUE(GetWebContents());
    host_observer = std::make_unique<content::test::PrerenderHostObserver>(
        *GetWebContents(), host_id);
    EXPECT_FALSE(host_observer->was_activated());
    tab_stats_tracker_->RemoveObserver(&mock_observer);
  }

  // OnPrimaryMainFrameNavigationCommitted() should be called after activating.
  {
    MockTabStatsObserver mock_observer;
    tab_stats_tracker_->AddObserverAndSetInitialState(&mock_observer);
    EXPECT_CALL(mock_observer, OnPrimaryMainFrameNavigationCommitted(_))
        .Times(1);
    prerender_test_helper().NavigatePrimaryPage(prerender_url);
    host_observer->WaitForActivation();
    EXPECT_TRUE(host_observer->was_activated());
    tab_stats_tracker_->RemoveObserver(&mock_observer);
  }
}

class TabStatsTrackerSubFrameBrowserTest : public TabStatsTrackerBrowserTest {
 public:
  TabStatsTrackerSubFrameBrowserTest() = default;
  ~TabStatsTrackerSubFrameBrowserTest() override = default;
  TabStatsTrackerSubFrameBrowserTest(
      const TabStatsTrackerSubFrameBrowserTest&) = delete;
  TabStatsTrackerSubFrameBrowserTest& operator=(
      const TabStatsTrackerSubFrameBrowserTest&) = delete;

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
    TabStatsTrackerBrowserTest::SetUpOnMainThread();
  }

 protected:
  content::test::FencedFrameTestHelper fenced_frame_helper_;
};

// Ensure that subframe navigation cannot affect TabStatsTracker.
IN_PROC_BROWSER_TEST_F(TabStatsTrackerSubFrameBrowserTest,
                       VerifyBehaviorOnSubFrameNavigation) {
  MockTabStatsObserver mock_observer;
  TestTabStatsObserver count_observer;
  tab_stats_tracker_->AddObserverAndSetInitialState(&mock_observer);
  tab_stats_tracker_->AddObserverAndSetInitialState(&count_observer);

  // Navigate to an initial page.
  ASSERT_TRUE(GetWebContents());
  EXPECT_CALL(mock_observer,
              OnPrimaryMainFrameNavigationCommitted(GetWebContents()))
      .Times(1);
  const GURL initial_url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(content::NavigateToURL(GetWebContents(), initial_url));
  EXPECT_EQ(1U, count_observer.interaction_count());
  ::testing::Mock::VerifyAndClear(&mock_observer);

  // Create an iframe and navigate inside the iframe.
  EXPECT_CALL(mock_observer, OnPrimaryMainFrameNavigationCommitted(_)).Times(0);
  ASSERT_TRUE(content::ExecJs(
      GetWebContents(),
      content::JsReplace("var iframe = document.createElement('iframe');"
                         "iframe.src = $1;"
                         "document.body.appendChild(iframe);",
                         embedded_test_server()->GetURL("/title1.html"))));
  WaitForLoadStop(GetWebContents());
  ::testing::Mock::VerifyAndClear(&mock_observer);

  // Create a fenced frame and navigate inside the fenced frame.
  EXPECT_CALL(mock_observer, OnPrimaryMainFrameNavigationCommitted(_)).Times(0);
  content::RenderFrameHost* fenced_frame_host =
      fenced_frame_helper_.CreateFencedFrame(
          GetWebContents()->GetPrimaryMainFrame(),
          embedded_test_server()->GetURL("/fenced_frames/title1.html"));
  ASSERT_NE(nullptr, fenced_frame_host);
  ::testing::Mock::VerifyAndClear(&mock_observer);

  tab_stats_tracker_->RemoveObserver(&mock_observer);
  tab_stats_tracker_->RemoveObserver(&count_observer);
  // Ensure that subframe navigation doesn't increase the user interaction
  // count.
  EXPECT_EQ(1U, count_observer.interaction_count());
}

}  // namespace metrics
