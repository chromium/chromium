// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_content_annotations/page_content_extraction_tab_strip_model_observer.h"

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/callback_list.h"
#include "base/files/file_path.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/statistics_recorder.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_manager_service.h"
#include "chrome/browser/ui/browser_manager_service_factory.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/test/base/testing_profile.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/os_crypt/async/browser/os_crypt_async.h"
#include "components/os_crypt/async/browser/test_utils.h"
#include "components/page_content_annotations/content/page_content_extraction_service.h"
#include "components/page_content_annotations/core/page_content_annotations_features.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/sessions/core/session_id.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace page_content_annotations {

namespace {

using ::testing::NiceMock;
using ::testing::Return;

class TestPageContentExtractionService : public PageContentExtractionService {
 public:
  TestPageContentExtractionService(os_crypt_async::OSCryptAsync* os_crypt_async,
                                   const base::FilePath& profile_path)
      : PageContentExtractionService(os_crypt_async,
                                     profile_path,
                                     /*tracker=*/nullptr) {}

  int64_t CacheContentForTab(content::WebContents* contents,
                             const std::string& title) {
    const int64_t tab_id = sessions::SessionTabHelper::IdForTab(contents).id();
    EXPECT_GT(tab_id, 0);
    contents->WasHidden();
    optimization_guide::proto::AnnotatedPageContent apc;
    apc.mutable_main_frame_data()->set_title(title);
    auto apc_ptr =
        base::MakeRefCounted<RefCountedAnnotatedPageContent>(std::move(apc));
    OnPageContentExtracted(contents->GetPrimaryPage(), std::move(apc_ptr),
                           /*screenshot_data=*/{}, tab_id);
    return tab_id;
  }
};

class ScopedMockBrowserWindow {
 public:
  explicit ScopedMockBrowserWindow(
      Profile* profile,
      BrowserWindowInterface::Type type = BrowserWindowInterface::TYPE_NORMAL)
      : tab_strip_model_(std::make_unique<TabStripModel>(&delegate_, profile)) {
    delegate_.SetBrowserWindowInterface(&browser_window_);
    ON_CALL(browser_window_, GetProfile()).WillByDefault(Return(profile));
    ON_CALL(browser_window_, GetType()).WillByDefault(Return(type));
    ON_CALL(browser_window_, GetTabStripModel())
        .WillByDefault(Return(tab_strip_model_.get()));
    ON_CALL(browser_window_, IsDeleteScheduled()).WillByDefault(Return(false));
    ON_CALL(browser_window_, RegisterBrowserDidClose)
        .WillByDefault(
            [this](BrowserWindowInterface::BrowserDidCloseCallback callback) {
              return browser_did_close_callbacks_.Add(std::move(callback));
            });

    BrowserManagerServiceFactory::GetForProfile(profile)->AddBrowserForTesting(
        &browser_window_);
  }

  ~ScopedMockBrowserWindow() { Close(); }

  ScopedMockBrowserWindow(const ScopedMockBrowserWindow&) = delete;
  ScopedMockBrowserWindow& operator=(const ScopedMockBrowserWindow&) = delete;

  void Close() {
    if (closed_) {
      return;
    }
    closed_ = true;
    browser_did_close_callbacks_.Notify(&browser_window_);
  }

  TabStripModel* tab_strip_model() { return tab_strip_model_.get(); }

 private:
  NiceMock<MockBrowserWindowInterface> browser_window_;
  TestTabStripModelDelegate delegate_;
  std::unique_ptr<TabStripModel> tab_strip_model_;
  base::RepeatingCallbackList<void(BrowserWindowInterface*)>
      browser_did_close_callbacks_;
  bool closed_ = false;
};

}  // namespace

class PageContentExtractionTabStripModelObserverTest : public testing::Test {
 public:
  PageContentExtractionTabStripModelObserverTest() {
    feature_list_.InitAndEnableFeature(features::kPageContentCache);
  }

  void SetUp() override {
    os_crypt_async_ = os_crypt_async::GetTestOSCryptAsyncForTesting();
    service_ = std::make_unique<TestPageContentExtractionService>(
        os_crypt_async_.get(), profile_.GetPath());
    // Wait for `OSCryptAsync` and SQLite store initialization
    // (`pending_tasks_`) to complete before test bodies issue cache operations.
    ASSERT_FALSE(GetCachedContentForTab(1).has_value());
  }

  void TearDown() override {
    browser_shutdown::SetTryingToQuit(false);
    observer_.reset();
    service_.reset();
  }

 protected:
  content::WebContents* AppendTabWithUrl(TabStripModel* tab_strip_model,
                                         const GURL& url,
                                         bool commit_navigation = true) {
    std::unique_ptr<content::WebContents> contents =
        content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);
    if (commit_navigation) {
      content::NavigationSimulator::NavigateAndCommitFromBrowser(contents.get(),
                                                                 url);
    } else {
      contents->GetController().LoadURL(
          url, content::Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
    }
    content::WebContents* raw_contents = contents.get();
    content::WebContentsTester::For(raw_contents)
        ->SetLastActiveTime(base::Time::Now());
    tab_strip_model->AppendWebContents(std::move(contents),
                                       /*foreground=*/true);
    return raw_contents;
  }

  std::optional<optimization_guide::proto::PageContext> GetCachedContentForTab(
      int64_t tab_id) {
    base::test::TestFuture<
        std::optional<optimization_guide::proto::PageContext>>
        future;
    service_->GetPageContentFromOnDiskCache(tab_id, future.GetCallback());
    return future.Take();
  }

  std::vector<int64_t> GetCachedTabIds() {
    base::test::TestFuture<std::vector<int64_t>> future;
    service_->GetOnDiskCachedTabIds(future.GetCallback());
    return future.Take();
  }

  void NotifySessionRestored(int num_tabs_restored) {
    observer_->OnSessionRestored(&profile_, num_tabs_restored);
  }

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  tabs::TabModel::PreventFeatureInitializationForTesting prevent_feature_init_;
  base::test::ScopedFeatureList feature_list_;
  TestingProfile profile_;
  std::unique_ptr<os_crypt_async::OSCryptAsync> os_crypt_async_;
  std::unique_ptr<TestPageContentExtractionService> service_;
  std::unique_ptr<PageContentExtractionTabStripModelObserver> observer_;
};

TEST_F(PageContentExtractionTabStripModelObserverTest,
       PostStartupReconciliationAndMetrics) {
  base::HistogramTester histogram_tester;

  // Instantiate `observer_` before any browser windows or tabs exist (matching
  // `Profile` initialization, where `PageContentExtractionService` is created
  // before `StartupBrowserCreator` opens initial tabs later in the same turn).
  observer_ = std::make_unique<PageContentExtractionTabStripModelObserver>(
      &profile_, service_.get());

  ScopedMockBrowserWindow window(&profile_);
  TabStripModel* model = window.tab_strip_model();

  // Tab 1: Active/eligible HTTP(S) tab with cached entry -> should be kept.
  content::WebContents* contents1 =
      AppendTabWithUrl(model, GURL("https://example.com/one"));
  const int64_t tab_id_1 = service_->CacheContentForTab(contents1, "one");

  // Tab 2: Uncommitted restored HTTP(S) tab (`GetLastCommittedURL()` is empty,
  // while `GetVisibleURL()` returns the pending entry URL) without cached
  // entry -> counted as not cached.
  content::WebContents* contents2 = AppendTabWithUrl(
      model, GURL("https://example.com/two"), /*commit_navigation=*/false);
  const int64_t tab_id_2 = sessions::SessionTabHelper::IdForTab(contents2).id();
  EXPECT_NE(tab_id_1, tab_id_2);

  // Tab 3: Open HTTP(S) tab whose last active time exceeds max cache age (7
  // days) -> treated as inactive/stale and evicted.
  content::WebContents* contents_expired =
      AppendTabWithUrl(model, GURL("https://example.com/expired"));
  content::WebContentsTester::For(contents_expired)
      ->SetLastActiveTime(base::Time::Now() - base::Days(8));
  const int64_t tab_id_expired =
      service_->CacheContentForTab(contents_expired, "expired");

  // Tab 4: Open non-HTTP(S) tab -> treated as ineligible/stale and evicted.
  content::WebContents* contents_non_http =
      AppendTabWithUrl(model, GURL("chrome://version"));
  const int64_t tab_id_non_http =
      service_->CacheContentForTab(contents_non_http, "non_http");

  // Stale closed tab that is no longer open in any `TabStripModel`.
  std::unique_ptr<content::WebContents> contents_stale_closed =
      content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);
  sessions::SessionTabHelper::CreateForWebContents(contents_stale_closed.get(),
                                                   base::NullCallback());
  const int64_t stale_closed_tab_id =
      service_->CacheContentForTab(contents_stale_closed.get(), "stale_closed");

  ASSERT_TRUE(GetCachedContentForTab(tab_id_1).has_value());
  ASSERT_TRUE(GetCachedContentForTab(tab_id_expired).has_value());
  ASSERT_TRUE(GetCachedContentForTab(tab_id_non_http).has_value());
  ASSERT_TRUE(GetCachedContentForTab(stale_closed_tab_id).has_value());

  base::StatisticsRecorder::HistogramWaiter metrics_waiter(
      "OptimizationGuide.PageContentCache.EligibleTabsCachedPercentage");
  base::StatisticsRecorder::HistogramWaiter delete_waiter(
      "OptimizationGuide.PageContentCache.DeletePageContentForTabsResult");

  // Fast-forward past the 25-second delayed cleanup task.
  task_environment_.FastForwardBy(base::Seconds(26));
  metrics_waiter.Wait();
  delete_waiter.Wait();

  // Verify only `tab_id_1` remains in the cache; the 3 stale entries were
  // evicted.
  EXPECT_TRUE(GetCachedContentForTab(tab_id_1).has_value());
  EXPECT_FALSE(GetCachedContentForTab(tab_id_expired).has_value());
  EXPECT_FALSE(GetCachedContentForTab(tab_id_non_http).has_value());
  EXPECT_FALSE(GetCachedContentForTab(stale_closed_tab_id).has_value());

  // Verify startup reconciliation histograms:
  // Eligible active tabs: {`tab_id_1`, `tab_id_2`} (2 total: 1 cached, 1 not
  // cached, 50% cached). Stale cached entries: {`tab_id_expired`,
  // `tab_id_non_http`, `stale_closed_tab_id`} (3 total).
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PageContentCache.TotalCacheSize", 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PageContentCache.CachedTabsCount", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PageContentCache.NotCachedTabsCount", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PageContentCache.StaleCacheEntriesCount", 3, 1);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PageContentCache.AvgPageSize", 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PageContentCache.EligibleTabsCachedPercentage", 50, 1);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PageContentCache.GetAllTabIdsLatency", 1);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PageContentCache.DeletePageContentForTabsLatency", 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PageContentCache.DeletePageContentForTabsResult", true,
      1);

  EXPECT_EQ(GetCachedTabIds(), std::vector<int64_t>{tab_id_1});
}

TEST_F(PageContentExtractionTabStripModelObserverTest,
       OnSessionRestoredRunsCleanUpOnce) {
  base::HistogramTester histogram_tester;

  observer_ = std::make_unique<PageContentExtractionTabStripModelObserver>(
      &profile_, service_.get());
  ScopedMockBrowserWindow window(&profile_);

  content::WebContents* contents1 = AppendTabWithUrl(
      window.tab_strip_model(), GURL("https://example.com/restored"));
  const int64_t tab_id_1 = service_->CacheContentForTab(contents1, "restored");
  std::unique_ptr<content::WebContents> stale_contents =
      content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);
  sessions::SessionTabHelper::CreateForWebContents(stale_contents.get(),
                                                   base::NullCallback());
  const int64_t stale_tab_id =
      service_->CacheContentForTab(stale_contents.get(), "stale");

  // Trigger `OnSessionRestored()` before the posted
  // `RunCleanUpTasksIfNotRestoring()` task runs; it should run
  // `RunCleanUpTasks()` and mark `has_run_cleanup_tasks_ = true` so the posted
  // task is a no-op when pumped.
  NotifySessionRestored(/*num_tabs_restored=*/1);

  base::StatisticsRecorder::HistogramWaiter metrics_waiter(
      "OptimizationGuide.PageContentCache.EligibleTabsCachedPercentage");
  base::StatisticsRecorder::HistogramWaiter delete_waiter(
      "OptimizationGuide.PageContentCache.DeletePageContentForTabsResult");
  task_environment_.FastForwardBy(base::Seconds(26));
  metrics_waiter.Wait();
  delete_waiter.Wait();

  EXPECT_TRUE(GetCachedContentForTab(tab_id_1).has_value());
  EXPECT_FALSE(GetCachedContentForTab(stale_tab_id).has_value());
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PageContentCache.CachedTabsCount", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PageContentCache.StaleCacheEntriesCount", 1, 1);

  // Calling `OnSessionRestored()` again after adding another tab should not
  // re-run startup reconciliation.
  AppendTabWithUrl(window.tab_strip_model(), GURL("https://example.com/later"));
  NotifySessionRestored(/*num_tabs_restored=*/1);
  task_environment_.FastForwardBy(base::Seconds(26));
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PageContentCache.CachedTabsCount", 1);
}

TEST_F(PageContentExtractionTabStripModelObserverTest,
       LastWindowCloseAllTabsPreservesCacheWhileSecondaryWindowCloseEvicts) {
  ScopedMockBrowserWindow window1(&profile_);
  content::WebContents* single_close_contents = AppendTabWithUrl(
      window1.tab_strip_model(), GURL("https://example.com/win1_single"));
  const int64_t single_close_tab_id =
      service_->CacheContentForTab(single_close_contents, "win1_single");
  content::WebContents* contents1 = AppendTabWithUrl(
      window1.tab_strip_model(), GURL("https://example.com/win1"));
  const int64_t window1_tab_id =
      service_->CacheContentForTab(contents1, "win1");

  // Create `observer_` after `window1` exists (exercising constructor
  // attachment via `ForEach`) and before `window2` is created (exercising
  // dynamic attachment via `OnBrowserCreated`).
  observer_ = std::make_unique<PageContentExtractionTabStripModelObserver>(
      &profile_, service_.get());

  // Closing an individual tab (`!closing_all()`) while `window1` is the only
  // open window should evict that tab's cached entry while preserving other
  // tabs in `window1`.
  window1.tab_strip_model()->CloseWebContentsAt(
      0, TabCloseTypes::CLOSE_USER_GESTURE);
  EXPECT_FALSE(GetCachedContentForTab(single_close_tab_id).has_value());
  EXPECT_TRUE(GetCachedContentForTab(window1_tab_id).has_value());

  ScopedMockBrowserWindow window2(&profile_);
  content::WebContents* contents2 = AppendTabWithUrl(
      window2.tab_strip_model(), GURL("https://example.com/win2"));
  const int64_t window2_tab_id =
      service_->CacheContentForTab(contents2, "win2");
  ASSERT_TRUE(GetCachedContentForTab(window2_tab_id).has_value());

  // Closing all tabs in secondary `window2` while `window1` stays open
  // should evict `window2`'s tabs.
  window2.tab_strip_model()->CloseAllTabs();
  window2.Close();

  EXPECT_TRUE(GetCachedContentForTab(window1_tab_id).has_value());
  EXPECT_FALSE(GetCachedContentForTab(window2_tab_id).has_value());

  // Quitting the app (`IsTryingToQuit()`) with multiple normal windows open
  // should preserve `window3`'s cached entries.
  ScopedMockBrowserWindow window3(&profile_);
  content::WebContents* contents3 = AppendTabWithUrl(
      window3.tab_strip_model(), GURL("https://example.com/win3"));
  const int64_t window3_tab_id =
      service_->CacheContentForTab(contents3, "win3");
  ASSERT_TRUE(GetCachedContentForTab(window3_tab_id).has_value());

  browser_shutdown::SetTryingToQuit(true);
  window3.tab_strip_model()->CloseAllTabs();
  window3.Close();
  browser_shutdown::SetTryingToQuit(false);
  EXPECT_TRUE(GetCachedContentForTab(window3_tab_id).has_value());

  // Replacing/discarding a tab's `WebContents` with a new `SessionID` should
  // migrate the cached entry from the old `SessionID` to the new `SessionID`.
  std::unique_ptr<content::WebContents> replacement_contents =
      content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);
  window1.tab_strip_model()->DiscardWebContents(
      contents1, std::move(replacement_contents));
  const int64_t replacement_tab_id =
      sessions::SessionTabHelper::IdForTab(
          window1.tab_strip_model()->GetWebContentsAt(0))
          .id();
  ASSERT_NE(replacement_tab_id, window1_tab_id);
  EXPECT_FALSE(GetCachedContentForTab(window1_tab_id).has_value());
  std::optional<optimization_guide::proto::PageContext> migrated =
      GetCachedContentForTab(replacement_tab_id);
  ASSERT_TRUE(migrated.has_value());
  EXPECT_EQ(migrated->annotated_page_content().main_frame_data().title(),
            "win1");

  // Closing all tabs in the last remaining `TYPE_NORMAL` `window1` for the
  // profile while a non-tabbed auxiliary window (such as DevTools) remains open
  // should still preserve `window1`'s cached entries across session restarts.
  ScopedMockBrowserWindow devtools_window(
      &profile_, BrowserWindowInterface::TYPE_DEVTOOLS);
  window1.tab_strip_model()->CloseAllTabs();
  window1.Close();

  EXPECT_TRUE(GetCachedContentForTab(replacement_tab_id).has_value());
}

}  // namespace page_content_annotations
