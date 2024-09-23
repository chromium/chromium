// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history_clusters/history_clusters_tab_helper.h"

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history_clusters/history_clusters_service_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/test/history_service_test_util.h"
#include "components/history_clusters/core/features.h"
#include "components/history_clusters/core/history_clusters_service_test_api.h"
#include "components/keyed_service/core/service_access_type.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Used to invoke a callback after `WebContentsDestroyed()` is invoked, but
// before the `WebContents` has been destroyed.
class OnDestroyWebContentsObserver : content::WebContentsObserver {
 public:
  OnDestroyWebContentsObserver(content::WebContents* web_contents,
                               base::OnceCallback<void()> callback)
      : callback_(std::move(callback)) {
    Observe(web_contents);
  }

 private:
  void WebContentsDestroyed() override { std::move(callback_).Run(); }

  base::OnceCallback<void()> callback_;
};

class HistoryClustersTabHelperTest : public ChromeRenderViewHostTestHarness {
 public:
  HistoryClustersTabHelperTest(const HistoryClustersTabHelperTest&) = delete;
  HistoryClustersTabHelperTest& operator=(const HistoryClustersTabHelperTest&) =
      delete;

 protected:
  HistoryClustersTabHelperTest() {
    // This needs to be initialized as early as possible to avoid data races
    // with tasks on other threads checking if features are enabled.
    feature_list_.InitAndEnableFeature(history_clusters::internal::kJourneys);
  }

  // ChromeRenderViewHostTestHarness:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    HistoryClustersTabHelper::CreateForWebContents(web_contents());
    helper_ = HistoryClustersTabHelper::FromWebContents(web_contents());
    ASSERT_TRUE(helper_);

    ASSERT_TRUE(history_service_ = HistoryServiceFactory::GetForProfile(
                    profile(), ServiceAccessType::IMPLICIT_ACCESS));

    history_clusters_service_test_api_ =
        std::make_unique<history_clusters::HistoryClustersServiceTestApi>(
            HistoryClustersServiceFactory::GetForBrowserContext(
                web_contents()->GetBrowserContext()),
            history_service_);
    ASSERT_TRUE(history_clusters_service_test_api_);

    BookmarkModelFactory::GetInstance()->SetTestingFactory(
        web_contents()->GetBrowserContext(),
        BookmarkModelFactory::GetDefaultFactory());
    ASSERT_TRUE(bookmark_model_ = BookmarkModelFactory::GetForBrowserContext(
                    web_contents()->GetBrowserContext()));
    ASSERT_TRUE(bookmark_model_);
    bookmarks::test::WaitForBookmarkModelToLoad(bookmark_model_);

    run_loop_quit_ = run_loop_.QuitClosure();
  }

  TestingProfile::TestingFactories GetTestingFactories() const override {
    return {TestingProfile::TestingFactory{
        HistoryServiceFactory::GetInstance(),
        HistoryServiceFactory::GetDefaultFactory()}};
  }

  std::vector<history::AnnotatedVisit> GetVisits() const {
    return history_clusters_service_test_api_->GetVisits();
  }

  base::Time MakeTime(int time_seconds) {
    return base::Time::FromDeltaSinceWindowsEpoch(base::Seconds(time_seconds));
  }

  void AddToHistory(const GURL& url, base::Time timestamp) {
    history::HistoryAddPageArgs add_page_args;
    add_page_args.url = url;
    add_page_args.title = u"Fake Title";
    add_page_args.time = timestamp;
    history_service_->AddPage(add_page_args);
  }

  void AddBookmark(const GURL& url) {
    ASSERT_TRUE(bookmark_model_->AddURL(bookmark_model_->bookmark_bar_node(), 0,
                                        std::u16string(), url));
  }

  base::test::ScopedFeatureList feature_list_;

  raw_ptr<HistoryClustersTabHelper, DanglingUntriaged> helper_;

  std::unique_ptr<history_clusters::HistoryClustersServiceTestApi>
      history_clusters_service_test_api_;

  base::CancelableTaskTracker tracker_;
  raw_ptr<history::HistoryService, DanglingUntriaged> history_service_;

  raw_ptr<bookmarks::BookmarkModel, DanglingUntriaged> bookmark_model_;

  // Used to verify the async callback is invoked.
  base::RunLoop run_loop_;
  base::RepeatingClosure run_loop_quit_;
};

// There are multiple events that occur with nondeterministic order:
// - History (w/ N visits): a history navigation occurs,
//   `OnUpdatedHistoryForNavigation()` is invoked, and a history query is made
//   which will (possibly after other events in the timeline) resolve with N
//   visits.
// - History resolve: the history query made above resolves.
// - Copy: the omnibox URL is copied and `OnOmniboxUrlCopied()` is invoked.
// - Expect UKM: UKM begins tracking a navigation and
//   `TagNavigationAsExpectingUkmNavigationComplete()` is invoked.
// - UKM: UKM ends tracking a navigation and `OnUkmNavigationComplete()` is
//   invoked.
// - Destroy: The `WebContents` is destroyed (i.e. the tab is closed) and
//   `WebContentsDestroyed()` is invoked.
// The below tests test different permutations of these events.

// History (w/ 0 visits) -> destroy
// When:
// 1) `OnUpdatedHistoryForNavigation()` is invoked but its history request isn't
//     resolved (because either the tab is closed too soon or there are no
//     matching visits).
// 2) `WebContentsDestroyed()` is invoked.
// Then: 0 context annotations should be committed.
TEST_F(HistoryClustersTabHelperTest, NavigationWith0HistoryVisits) {
  AddToHistory(GURL{"https://other.com"}, MakeTime(2));
  helper_->OnUpdatedHistoryForNavigation(0, MakeTime(1),
                                         GURL{"https://github.com"});
  history::BlockUntilHistoryProcessesPendingRequests(history_service_);
  helper_->OnOmniboxUrlCopied();
  DeleteContents();

  auto visits = GetVisits();
  EXPECT_EQ(visits.size(), 1U);
  // Context signals should be unavailable because there's no matching visit.
  EXPECT_FALSE(visits[0].context_annotations.omnibox_url_copied);
}

// History (w/ 1 visit) -> destroy
// When:
// 1) `OnUpdatedHistoryForNavigation()` is invoked and 1 history visit are
//    fetched.
// 2) `WebContentsDestroyed()` is invoked.
// Then: 1 context annotation should be committed.
TEST_F(HistoryClustersTabHelperTest, NavigationWith1HistoryVisits) {
  AddToHistory(GURL{"https://github.com"}, MakeTime(1));
  helper_->OnUpdatedHistoryForNavigation(0, MakeTime(1),
                                         GURL{"https://github.com"});
  history::BlockUntilHistoryProcessesPendingRequests(history_service_);
  helper_->OnOmniboxUrlCopied();
  EXPECT_EQ(GetVisits().size(), 1u);

  DeleteContents();
  // Adding the bookmark after the WebContents is destroyed should do nothing.
  AddBookmark(GURL{"https://github.com"});

  auto visits = GetVisits();
  ASSERT_EQ(visits.size(), 1u);
  EXPECT_EQ(visits[0].url_row.url(), GURL{"https://github.com"});
  EXPECT_EQ(visits[0].context_annotations.duration_since_last_visit.InSeconds(),
            -1);
  EXPECT_FALSE(visits[0].context_annotations.is_new_bookmark);
  EXPECT_TRUE(visits[0].context_annotations.omnibox_url_copied);
}

// History (w/ 1 *mismatched* visit) -> destroy
// When:
// 1) `OnUpdatedHistoryForNavigation()` is invoked and 1 history visit is
//    fetched, but it doesn't match the navigation timestamp.
// 2) `WebContentsDestroyed()` is invoked.
// Then: 0 context annotations should be committed.
TEST_F(HistoryClustersTabHelperTest, NavigationWith1MismatchedHistoryVisit) {
  // One pre-existing visit to the URL exists in the DB.
  AddToHistory(GURL{"https://github.com"}, MakeTime(1));
  // A new visit to the same URL happens, but this one is *not* in the DB.
  helper_->OnUpdatedHistoryForNavigation(0, MakeTime(100),
                                         GURL{"https://github.com"});
  history::BlockUntilHistoryProcessesPendingRequests(history_service_);
  helper_->OnOmniboxUrlCopied();
  ASSERT_EQ(GetVisits().size(), 1u);

  DeleteContents();

  auto visits = GetVisits();
  ASSERT_EQ(visits.size(), 1u);
  ASSERT_EQ(visits[0].url_row.url(), GURL{"https://github.com"});
  EXPECT_NE(visits[0].visit_row.visit_time, MakeTime(100));
  // Since the timestamps didn't match, the visit should *not* have gotten any
  // annotations.
  EXPECT_FALSE(visits[0].context_annotations.omnibox_url_copied);
}

// History (w/ 2 visits) -> destroy
// When:
// 1) `OnUpdatedHistoryForNavigation()` is invoked and 2 history visits are
//    fetched.
// 2) `WebContentsDestroyed()` is invoked.
// Then: 1 context annotation should be committed.
TEST_F(HistoryClustersTabHelperTest, NavigationWith2HistoryVisits) {
  AddToHistory(GURL{"https://github.com"}, MakeTime(19));
  AddToHistory(GURL{"https://github.com"}, MakeTime(23));
  helper_->OnUpdatedHistoryForNavigation(0, MakeTime(23),
                                         GURL{"https://github.com"});
  history::BlockUntilHistoryProcessesPendingRequests(history_service_);
  auto visits = GetVisits();
  // Two visits are there but context annotations are initially unavailable.
  EXPECT_EQ(visits.size(), 2u);
  EXPECT_EQ(visits[0].context_annotations.duration_since_last_visit.InSeconds(),
            -1);

  DeleteContents();
  visits = GetVisits();
  ASSERT_EQ(visits.size(), 2u);
  EXPECT_EQ(visits[0].url_row.url(), GURL{"https://github.com"});
  EXPECT_EQ(visits[0].context_annotations.duration_since_last_visit.InSeconds(),
            4);
  EXPECT_EQ(visits[1].url_row.url(), GURL{"https://github.com"});
  EXPECT_EQ(visits[1].context_annotations.duration_since_last_visit.InSeconds(),
            -1);
}

// History (w/ 0 visits) -> history (w/ 0 visits) -> destroy
// When:
// 1) `OnUpdatedHistoryForNavigation()` is invoked but its history request isn't
//     resolved (because either the tab is closed too soon or there are no
//     matching visits).
// 2) `OnUpdatedHistoryForNavigation()` is invoked but its history request isn't
//     resolved (because either the tab is closed too soon or there are no
//     matching visits).
// 3) `WebContentsDestroyed()` is invoked.
// Then: 0 visits should be committed.
TEST_F(HistoryClustersTabHelperTest, TwoNavigationsWith0HistoryVisits) {
  helper_->OnUpdatedHistoryForNavigation(0, MakeTime(1),
                                         GURL{"https://github.com"});
  helper_->OnUpdatedHistoryForNavigation(1, MakeTime(2),
                                         GURL{"https://google.com"});
  history::BlockUntilHistoryProcessesPendingRequests(history_service_);
  EXPECT_TRUE(GetVisits().empty());
}

// History (w/ 2 visits) -> history (w/ 2 visits) -> destroy
// When:
// 1) `OnUpdatedHistoryForNavigation()` is invoked and 2 history visits are
//    fetched.
// 2) `OnUpdatedHistoryForNavigation()` is invoked and 2 history visits are
//    fetched.
// 3) `WebContentsDestroyed()` is invoked.
// Then: 2 context annotations should be committed.
TEST_F(HistoryClustersTabHelperTest, TwoNavigationsWith2HistoryVisits) {
  AddToHistory(GURL{"https://github.com"}, MakeTime(1));
  AddToHistory(GURL{"https://github.com"}, MakeTime(5));
  AddToHistory(GURL{"https://google.com"}, MakeTime(10));
  AddToHistory(GURL{"https://google.com"}, MakeTime(18));
  helper_->OnUpdatedHistoryForNavigation(0, MakeTime(5),
                                         GURL{"https://github.com"});
  history::BlockUntilHistoryProcessesPendingRequests(history_service_);
  // 4 visits are in History, but they don't have context annotations.
  auto visits = GetVisits();
  ASSERT_EQ(visits.size(), 4u);
  EXPECT_EQ(visits[0].url_row.url(), GURL{"https://google.com"});
  EXPECT_EQ(visits[0].context_annotations.duration_since_last_visit.InSeconds(),
            -1);
  EXPECT_EQ(visits[1].url_row.url(), GURL{"https://google.com"});
  EXPECT_EQ(visits[1].context_annotations.duration_since_last_visit.InSeconds(),
            -1);
  EXPECT_EQ(visits[2].url_row.url(), GURL{"https://github.com"});
  EXPECT_EQ(visits[2].context_annotations.duration_since_last_visit.InSeconds(),
            -1);
  EXPECT_EQ(visits[3].url_row.url(), GURL{"https://github.com"});
  EXPECT_EQ(visits[3].context_annotations.duration_since_last_visit.InSeconds(),
            -1);

  helper_->OnUpdatedHistoryForNavigation(1, MakeTime(18),
                                         GURL{"https://google.com"});
  history::BlockUntilHistoryProcessesPendingRequests(history_service_);
  visits = GetVisits();
  ASSERT_EQ(visits.size(), 4u);
  EXPECT_EQ(visits[0].context_annotations.duration_since_last_visit.InSeconds(),
            -1);
  EXPECT_EQ(visits[1].context_annotations.duration_since_last_visit.InSeconds(),
            -1);
  EXPECT_EQ(visits[2].context_annotations.duration_since_last_visit.InSeconds(),
            4);
  EXPECT_EQ(visits[3].context_annotations.duration_since_last_visit.InSeconds(),
            -1);

  DeleteContents();
  visits = GetVisits();
  ASSERT_EQ(visits.size(), 4u);
  EXPECT_EQ(visits[0].context_annotations.duration_since_last_visit.InSeconds(),
            8);
  EXPECT_EQ(visits[1].context_annotations.duration_since_last_visit.InSeconds(),
            -1);
  EXPECT_EQ(visits[2].context_annotations.duration_since_last_visit.InSeconds(),
            4);
  EXPECT_EQ(visits[3].context_annotations.duration_since_last_visit.InSeconds(),
            -1);
}

// For the remaining tests, all navigations will have at least 1 history visit.

// History -> destroy -> history resolve
// When:
// 1) `OnUpdatedHistoryForNavigation()` is invoked.
// 2) `WebContentsDestroyed()` is invoked before the previous history request is
//    resolved.
// Then: 0 context annotations should be committed.
TEST_F(HistoryClustersTabHelperTest, HistoryResolvedAfterDestroy) {
  AddToHistory(GURL{"https://github.com"}, MakeTime(1));
  helper_->OnUpdatedHistoryForNavigation(0, MakeTime(1),
                                         GURL{"https://github.com"});
  helper_->OnOmniboxUrlCopied();
  DeleteContents();
  history::BlockUntilHistoryProcessesPendingRequests(history_service_);

  // Validate that the omnibox-url-copied signal was lost.
  auto visits = GetVisits();
  ASSERT_EQ(visits.size(), 1u);
  EXPECT_FALSE(visits[0].context_annotations.omnibox_url_copied);
}

// History -> history -> history resolve -> destroy
// When:
// 1) `OnUpdatedHistoryForNavigation()` is invoked.
// 2) `OnUpdatedHistoryForNavigation()` is invoked before the previous history
//    request is resolved.
// 3) `WebContentsDestroyed()` is invoked.
// Then: 2 context annotations should be committed.
TEST_F(HistoryClustersTabHelperTest, HistoryResolvedAfter2ndNavigation) {
  AddToHistory(GURL{"https://google.com"}, MakeTime(1));
  AddToHistory(GURL{"https://github.com"}, MakeTime(2));
  helper_->OnUpdatedHistoryForNavigation(0, MakeTime(1),
                                         GURL{"https://google.com"});
  helper_->OnUpdatedHistoryForNavigation(1, MakeTime(2),
                                         GURL{"https://github.com"});

  // Bookmarked after navigation ends, but before its history request resolved.
  AddBookmark(GURL{"https://google.com"});

  auto visits = GetVisits();
  ASSERT_EQ(visits.size(), 2u);
  EXPECT_EQ(visits[0].url_row.url(), GURL{"https://github.com"});
  EXPECT_FALSE(visits[0].context_annotations.is_new_bookmark);
  EXPECT_EQ(visits[1].url_row.url(), GURL{"https://google.com"});
  EXPECT_FALSE(visits[1].context_annotations.is_new_bookmark);

  history::BlockUntilHistoryProcessesPendingRequests(history_service_);
  DeleteContents();
  // Bookmarked after navigation ends and its history request resolved.
  AddBookmark(GURL{"https://github.com"});
  visits = GetVisits();
  ASSERT_EQ(visits.size(), 2u);
  EXPECT_FALSE(visits[0].context_annotations.is_new_bookmark);
  EXPECT_TRUE(visits[1].context_annotations.is_new_bookmark);
}

// History -> copy -> history resolve -> history -> history -> copy -> destroy
// When:
// 1) `OnUpdatedHistoryForNavigation()` is invoked.
// 2) `OnOmniboxUrlCopied()` is invoked before the previous history request is
//    resolved.
// 3) `OnUpdatedHistoryForNavigation()` is invoked.
// 4) `OnUpdatedHistoryForNavigation()` is invoked.
// 5) `OnOmniboxUrlCopied()` is invoked after the previous history request is
//    resolved
// 6) `WebContentsDestroyed()` is invoked.
// Then: 3 context annotations should be committed; the 1st and 3rd should have
//       `omnibox_url_copied` true.
TEST_F(HistoryClustersTabHelperTest, UrlsCopied) {
  AddToHistory(GURL{"https://github.com"}, MakeTime(1));
  AddToHistory(GURL{"https://google.com"}, MakeTime(2));
  AddToHistory(GURL{"https://gmail.com"}, MakeTime(3));
  helper_->OnUpdatedHistoryForNavigation(0, MakeTime(1),
                                         GURL{"https://github.com"});
  helper_->OnOmniboxUrlCopied();
  history::BlockUntilHistoryProcessesPendingRequests(history_service_);
  auto visits = GetVisits();
  ASSERT_EQ(visits.size(), 3u);
  EXPECT_EQ(visits[0].url_row.url(), GURL{"https://gmail.com"});
  EXPECT_FALSE(visits[0].context_annotations.omnibox_url_copied);
  EXPECT_EQ(visits[1].url_row.url(), GURL{"https://google.com"});
  EXPECT_FALSE(visits[1].context_annotations.omnibox_url_copied);
  EXPECT_EQ(visits[2].url_row.url(), GURL{"https://github.com"});
  EXPECT_FALSE(visits[2].context_annotations.omnibox_url_copied);

  helper_->OnUpdatedHistoryForNavigation(1, MakeTime(2),
                                         GURL{"https://google.com"});
  history::BlockUntilHistoryProcessesPendingRequests(history_service_);
  visits = GetVisits();
  ASSERT_EQ(visits.size(), 3u);
  EXPECT_EQ(visits[0].url_row.url(), GURL{"https://gmail.com"});
  EXPECT_FALSE(visits[0].context_annotations.omnibox_url_copied);
  EXPECT_EQ(visits[1].url_row.url(), GURL{"https://google.com"});
  EXPECT_FALSE(visits[1].context_annotations.omnibox_url_copied);
  EXPECT_EQ(visits[2].url_row.url(), GURL{"https://github.com"});
  EXPECT_TRUE(visits[2].context_annotations.omnibox_url_copied);

  helper_->OnUpdatedHistoryForNavigation(2, MakeTime(3),
                                         GURL{"https://gmail.com"});
  history::BlockUntilHistoryProcessesPendingRequests(history_service_);
  helper_->OnOmniboxUrlCopied();
  visits = GetVisits();
  ASSERT_EQ(visits.size(), 3u);
  EXPECT_EQ(visits[0].url_row.url(), GURL{"https://gmail.com"});
  EXPECT_FALSE(visits[0].context_annotations.omnibox_url_copied);
  EXPECT_EQ(visits[1].url_row.url(), GURL{"https://google.com"});
  EXPECT_FALSE(visits[1].context_annotations.omnibox_url_copied);
  EXPECT_EQ(visits[2].url_row.url(), GURL{"https://github.com"});
  EXPECT_TRUE(visits[2].context_annotations.omnibox_url_copied);

  DeleteContents();
  visits = GetVisits();
  ASSERT_EQ(visits.size(), 3u);
  EXPECT_EQ(visits[0].url_row.url(), GURL{"https://gmail.com"});
  EXPECT_TRUE(visits[0].context_annotations.omnibox_url_copied);
  EXPECT_EQ(visits[1].url_row.url(), GURL{"https://google.com"});
  EXPECT_FALSE(visits[1].context_annotations.omnibox_url_copied);
  EXPECT_EQ(visits[2].url_row.url(), GURL{"https://github.com"});
  EXPECT_TRUE(visits[2].context_annotations.omnibox_url_copied);
}

// History -> expect UKM -> UKM -> destroy
// When:
// 1) `OnUpdatedHistoryForNavigation()` is invoked.
// 2) `TagNavigationAsExpectingUkmNavigationComplete()` is invoked.
// 3) `OnUkmNavigationComplete()` is invoked.
// 4) `WebContentsDestroyed()` is invoked.
// Then: 1 context annotations committed after step 3 w/ a `page_end_reason`.
TEST_F(HistoryClustersTabHelperTest, NavigationWithUkmBeforeDestroy) {
  AddToHistory(GURL{"https://github.com"}, MakeTime(1));
  helper_->OnUpdatedHistoryForNavigation(0, MakeTime(1),
                                         GURL{"https://github.com"});
  history::BlockUntilHistoryProcessesPendingRequests(history_service_);
  helper_->TagNavigationAsExpectingUkmNavigationComplete(0);
  ASSERT_EQ(GetVisits().size(), 1u);
  EXPECT_EQ(GetVisits()[0].url_row.url(), GURL{"https://github.com"});
  EXPECT_EQ(GetVisits()[0].context_annotations.page_end_reason, 0);
  helper_->OnUkmNavigationComplete(
      0,
      /*total_foreground_duration=*/base::Seconds(20),
      page_load_metrics::PageEndReason::END_OTHER);
  ASSERT_EQ(GetVisits().size(), 1u);
  EXPECT_EQ(GetVisits()[0].url_row.url(), GURL{"https://github.com"});
  EXPECT_EQ(GetVisits()[0].context_annotations.page_end_reason,
            page_load_metrics::PageEndReason::END_OTHER);
  EXPECT_EQ(GetVisits()[0].context_annotations.total_foreground_duration,
            base::Seconds(20));
  DeleteContents();
}

// History -> expect UKM -> destroy -> UKM
// When:
// 1) `OnUpdatedHistoryForNavigation()` is invoked.
// 2) `TagNavigationAsExpectingUkmNavigationComplete()` is invoked.
// 3) `WebContentsDestroyed()` is invoked.
// 4) `OnUkmNavigationComplete()` is invoked.
// Then: 1 context annotation should be committed after step 4 w/ a
//       `page_end_reason`.
TEST_F(HistoryClustersTabHelperTest, NavigationWithUkmAfterDestroy) {
  AddToHistory(GURL{"https://github.com"}, MakeTime(1));
  helper_->OnUpdatedHistoryForNavigation(0, MakeTime(1),
                                         GURL{"https://github.com"});
  history::BlockUntilHistoryProcessesPendingRequests(history_service_);
  helper_->TagNavigationAsExpectingUkmNavigationComplete(0);

  // Invoke `OnUkmNavigationComplete()` after `WebContentsDestroyed()` is
  // invoked, but before the `WebContents` has been destroyed.
  ASSERT_EQ(GetVisits().size(), 1u);
  EXPECT_EQ(GetVisits()[0].url_row.url(), GURL{"https://github.com"});
  EXPECT_EQ(GetVisits()[0].context_annotations.page_end_reason, 0);
  OnDestroyWebContentsObserver test_web_contents_observer(
      web_contents(), base::BindLambdaForTesting([&]() {
        EXPECT_EQ(GetVisits()[0].context_annotations.page_end_reason, 0);
        helper_->OnUkmNavigationComplete(
            0, base::Seconds(20), page_load_metrics::PageEndReason::END_OTHER);
        run_loop_quit_.Run();
      }));

  DeleteContents();
  run_loop_.Run();
  ASSERT_EQ(GetVisits().size(), 1u);
  EXPECT_EQ(GetVisits()[0].url_row.url(), GURL{"https://github.com"});
  EXPECT_EQ(GetVisits()[0].context_annotations.page_end_reason,
            page_load_metrics::PageEndReason::END_OTHER);
  EXPECT_EQ(GetVisits()[0].context_annotations.total_foreground_duration,
            base::Seconds(20));
}

// Expect UKM -> history -> UKM -> destroy
// When:
// 1) `TagNavigationAsExpectingUkmNavigationComplete()` is invoked.
// 2) `OnUpdatedHistoryForNavigation()` is invoked.
// 3) `OnUkmNavigationComplete()` is invoked.
// 4) `WebContentsDestroyed()` is invoked.
// Then: 1 context annotation should be committed after step 3 w/ a
//       `page_end_reason`.
TEST_F(HistoryClustersTabHelperTest,
       NavigationAfterUkmExpectAndWithUkmBeforeDestroy) {
  AddToHistory(GURL{"https://github.com"}, MakeTime(1));
  helper_->TagNavigationAsExpectingUkmNavigationComplete(0);
  helper_->OnUpdatedHistoryForNavigation(0, MakeTime(1),
                                         GURL{"https://github.com"});
  history::BlockUntilHistoryProcessesPendingRequests(history_service_);
  ASSERT_EQ(GetVisits().size(), 1u);
  EXPECT_EQ(GetVisits()[0].url_row.url(), GURL{"https://github.com"});
  EXPECT_EQ(GetVisits()[0].context_annotations.page_end_reason, 0);
  helper_->OnUkmNavigationComplete(0, base::Seconds(20),
                                   page_load_metrics::PageEndReason::END_OTHER);
  ASSERT_EQ(GetVisits().size(), 1u);
  EXPECT_EQ(GetVisits()[0].url_row.url(), GURL{"https://github.com"});
  EXPECT_EQ(GetVisits()[0].context_annotations.page_end_reason,
            page_load_metrics::PageEndReason::END_OTHER);
  EXPECT_EQ(GetVisits()[0].context_annotations.total_foreground_duration,
            base::Seconds(20));
  DeleteContents();
}

// History -> expect UKM -> UKM -> destroy -> history resolve
// When:
// 1) `OnUpdatedHistoryForNavigation()` is invoked.
// 2) `TagNavigationAsExpectingUkmNavigationComplete()` is invoked.
// 3) `OnUkmNavigationComplete()` is invoked.
// 4) `WebContentsDestroyed()` is invoked before the previous history request is
//    resolved.
// Then: 1 context annotation should be committed after step 4 w/ a
// `page_end_reason`.
TEST_F(HistoryClustersTabHelperTest,
       NavigationWithUkmBeforeDestroyAndHistoryResolvedAfterDestroy) {
  AddToHistory(GURL{"https://github.com"}, MakeTime(1));
  helper_->OnUpdatedHistoryForNavigation(0, MakeTime(1),
                                         GURL{"https://github.com"});
  helper_->TagNavigationAsExpectingUkmNavigationComplete(0);
  AddBookmark(GURL{"https://github.com"});
  helper_->OnUkmNavigationComplete(0, base::Seconds(20),
                                   page_load_metrics::PageEndReason::END_OTHER);
  auto visits = GetVisits();
  ASSERT_EQ(visits.size(), 1u);
  EXPECT_EQ(visits[0].url_row.url(), GURL{"https://github.com"});
  EXPECT_FALSE(visits[0].context_annotations.is_new_bookmark);
  EXPECT_EQ(visits[0].context_annotations.page_end_reason, 0);
  EXPECT_EQ(visits[0].context_annotations.total_foreground_duration,
            base::Seconds(-1));

  // Resolve the history request after `WebContentsDestroyed()` is invoked, but
  // before the `WebContents` has been destroyed.
  OnDestroyWebContentsObserver test_web_contents_observer(
      web_contents(), base::BindLambdaForTesting([&]() {
        history::BlockUntilHistoryProcessesPendingRequests(history_service_);
        run_loop_quit_.Run();
      }));

  DeleteContents();
  run_loop_.Run();
  visits = GetVisits();
  ASSERT_EQ(visits.size(), 1u);
  EXPECT_EQ(visits[0].url_row.url(), GURL{"https://github.com"});
  EXPECT_TRUE(visits[0].context_annotations.is_new_bookmark);
  EXPECT_EQ(visits[0].context_annotations.page_end_reason,
            page_load_metrics::PageEndReason::END_OTHER);
  EXPECT_EQ(visits[0].context_annotations.total_foreground_duration,
            base::Seconds(20));
}

// Expect History -> expect UKM 1 -> UKM 1 -> history -> destroy
// When:
// 1) `OnUpdatedHistoryForNavigation()` is invoked.
// 2) `TagNavigationAsExpectingUkmNavigationComplete()` is invoked for the above
//    navigation.
// 3) `OnUkmNavigationComplete()` is invoked for the above navigation.
// 4) `OnUpdatedHistoryForNavigation()` is invoked.
// 5) `WebContentsDestroyed()` is invoked.
// Then: 2 context annotations should be committed after steps 3 and 5; the 1st
//.      should have a `page_end_reason`.
TEST_F(HistoryClustersTabHelperTest,
       TwoNavigationsWith1stUkmBefore2ndNavigation) {
  AddToHistory(GURL{"https://google.com"}, MakeTime(1));
  AddToHistory(GURL{"https://github.com"}, MakeTime(2));
  helper_->OnUpdatedHistoryForNavigation(0, MakeTime(2),
                                         GURL{"https://github.com"});
  history::BlockUntilHistoryProcessesPendingRequests(history_service_);
  helper_->TagNavigationAsExpectingUkmNavigationComplete(0);

  auto visits = GetVisits();
  ASSERT_EQ(visits.size(), 2u);
  EXPECT_EQ(visits[0].url_row.url(), GURL{"https://github.com"});
  EXPECT_EQ(visits[0].context_annotations.page_end_reason, 0);
  EXPECT_EQ(visits[1].url_row.url(), GURL{"https://google.com"});
  EXPECT_EQ(visits[1].context_annotations.page_end_reason, 0);
  helper_->OnUkmNavigationComplete(0, base::Seconds(20),
                                   page_load_metrics::PageEndReason::END_OTHER);

  helper_->OnUpdatedHistoryForNavigation(1, MakeTime(1),
                                         GURL{"https://google.com"});
  history::BlockUntilHistoryProcessesPendingRequests(history_service_);

  DeleteContents();
  visits = GetVisits();
  ASSERT_EQ(visits.size(), 2u);
  EXPECT_EQ(visits[0].url_row.url(), GURL{"https://github.com"});
  EXPECT_EQ(visits[0].context_annotations.page_end_reason,
            page_load_metrics::PageEndReason::END_OTHER);
  EXPECT_EQ(visits[0].context_annotations.total_foreground_duration,
            base::Seconds(20));
  EXPECT_EQ(visits[1].url_row.url(), GURL{"https://google.com"});
  EXPECT_EQ(visits[1].context_annotations.page_end_reason, 0);
}

// Expect History -> Expect UKM 1 -> history -> UKM 1 -> destroy
// When:
// 1) `OnUpdatedHistoryForNavigation()` is invoked.
// 2) `TagNavigationAsExpectingUkmNavigationComplete()` is invoked for the above
//    navigation.
// 3) `OnUpdatedHistoryForNavigation()` is invoked.
// 4) `OnUkmNavigationComplete()` is invoked for the 1st navigation.
// 5) `WebContentsDestroyed()` is invoked.
// Then: 2 context annotations should be committed after steps 4 and 5; the 1st
//.      should have a `page_end_reason`.
TEST_F(HistoryClustersTabHelperTest,
       TwoNavigationsWith1stUkmAfter2ndNavigation) {
  AddToHistory(GURL{"https://google.com"}, MakeTime(1));
  AddToHistory(GURL{"https://github.com"}, MakeTime(2));
  helper_->OnUpdatedHistoryForNavigation(0, MakeTime(2),
                                         GURL{"https://github.com"});
  history::BlockUntilHistoryProcessesPendingRequests(history_service_);
  helper_->TagNavigationAsExpectingUkmNavigationComplete(0);
  helper_->OnUpdatedHistoryForNavigation(1, MakeTime(1),
                                         GURL{"https://google.com"});
  history::BlockUntilHistoryProcessesPendingRequests(history_service_);
  auto visits = GetVisits();
  ASSERT_EQ(visits.size(), 2u);
  EXPECT_EQ(visits[0].url_row.url(), GURL{"https://github.com"});
  EXPECT_EQ(visits[0].context_annotations.page_end_reason, 0);
  EXPECT_EQ(visits[1].url_row.url(), GURL{"https://google.com"});
  EXPECT_EQ(visits[1].context_annotations.page_end_reason, 0);

  helper_->OnUkmNavigationComplete(0, base::Seconds(20),
                                   page_load_metrics::PageEndReason::END_OTHER);
  visits = GetVisits();
  ASSERT_EQ(visits.size(), 2u);
  EXPECT_EQ(visits[0].url_row.url(), GURL{"https://github.com"});
  EXPECT_EQ(visits[0].context_annotations.page_end_reason,
            page_load_metrics::PageEndReason::END_OTHER);
  EXPECT_EQ(visits[0].context_annotations.total_foreground_duration,
            base::Seconds(20));
  EXPECT_EQ(visits[1].url_row.url(), GURL{"https://google.com"});
  EXPECT_EQ(visits[1].context_annotations.page_end_reason, 0);
  EXPECT_EQ(visits[1].context_annotations.total_foreground_duration,
            base::Seconds(-1));

  DeleteContents();
  visits = GetVisits();
  ASSERT_EQ(visits.size(), 2u);
  EXPECT_EQ(visits[0].url_row.url(), GURL{"https://github.com"});
  EXPECT_EQ(visits[0].context_annotations.page_end_reason,
            page_load_metrics::PageEndReason::END_OTHER);
  EXPECT_EQ(visits[0].context_annotations.total_foreground_duration,
            base::Seconds(20));
  EXPECT_EQ(visits[1].url_row.url(), GURL{"https://google.com"});
  EXPECT_EQ(visits[1].context_annotations.page_end_reason, 0);
  EXPECT_EQ(visits[1].context_annotations.total_foreground_duration,
            base::Seconds(-1));
}

// Expect History -> Expect UKM 2 -> history -> destroy -> UKM 2
// When:
// 1) `OnUpdatedHistoryForNavigation()` is invoked.
// 2) `TagNavigationAsExpectingUkmNavigationComplete()` is invoked for the below
//    navigation.
// 3) `OnUpdatedHistoryForNavigation()` is invoked.
// 4) `WebContentsDestroyed()` is invoked.
// 5) `OnUkmNavigationComplete()` is invoked for the 2nd navigation.
// Then: 2 context annotations should be committed after steps 2 and 5; the 2nd
//.      should have a `page_end_reason`.
TEST_F(HistoryClustersTabHelperTest, TwoNavigations2ndUkmBefore2ndNavigation) {
  AddToHistory(GURL{"https://google.com"}, MakeTime(1));
  AddToHistory(GURL{"https://github.com"}, MakeTime(2));
  helper_->OnUpdatedHistoryForNavigation(0, MakeTime(2),
                                         GURL{"https://github.com"});
  history::BlockUntilHistoryProcessesPendingRequests(history_service_);
  auto visits = GetVisits();
  ASSERT_EQ(visits.size(), 2u);
  EXPECT_EQ(visits[0].url_row.url(), GURL{"https://github.com"});
  EXPECT_EQ(visits[0].context_annotations.page_end_reason, 0);
  EXPECT_EQ(visits[0].context_annotations.total_foreground_duration,
            base::Seconds(-1));
  EXPECT_EQ(visits[1].url_row.url(), GURL{"https://google.com"});
  EXPECT_EQ(visits[1].context_annotations.page_end_reason, 0);
  EXPECT_EQ(visits[1].context_annotations.total_foreground_duration,
            base::Seconds(-1));

  helper_->TagNavigationAsExpectingUkmNavigationComplete(1);
  EXPECT_EQ(GetVisits().size(), 2u);

  helper_->OnUpdatedHistoryForNavigation(1, MakeTime(1),
                                         GURL{"https://google.com"});
  history::BlockUntilHistoryProcessesPendingRequests(history_service_);

  // Invoke `OnUkmNavigationComplete()` after `WebContentsDestroyed()` is
  // invoked, but before the `WebContents` has been destroyed.
  EXPECT_EQ(GetVisits().size(), 2u);
  OnDestroyWebContentsObserver test_web_contents_observer(
      web_contents(), base::BindLambdaForTesting([&]() {
        EXPECT_EQ(GetVisits().size(), 2u);
        helper_->OnUkmNavigationComplete(
            1, base::Seconds(20), page_load_metrics::PageEndReason::END_OTHER);
        run_loop_quit_.Run();
      }));

  DeleteContents();
  visits = GetVisits();
  ASSERT_EQ(visits.size(), 2u);
  EXPECT_EQ(visits[0].url_row.url(), GURL{"https://github.com"});
  EXPECT_EQ(visits[0].context_annotations.page_end_reason, 0);
  EXPECT_EQ(visits[0].context_annotations.total_foreground_duration,
            base::Seconds(-1));
  EXPECT_EQ(visits[1].url_row.url(), GURL{"https://google.com"});
  EXPECT_EQ(visits[1].context_annotations.page_end_reason,
            page_load_metrics::PageEndReason::END_OTHER);
  EXPECT_EQ(visits[1].context_annotations.total_foreground_duration,
            base::Seconds(20));
}

}  // namespace
