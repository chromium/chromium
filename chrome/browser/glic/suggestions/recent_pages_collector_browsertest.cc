// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/suggestions/recent_pages_collector.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/time/time_override.h"
#include "chrome/browser/glic/suggestions/contextual_cueing_features.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/search_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/history/core/browser/history_backend.h"
#include "components/history/core/browser/history_db_task.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/sync/history_backend_for_sync.h"
#include "components/history/core/test/history_service_test_util.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace glic {
namespace {

using Pages = std::vector<optimization_guide::proto::Tab>;

// Adds a visit synced from another device; AddPageWithDetails can't set
// originator_cache_guid.
class AddSyncedVisitTask : public history::HistoryDBTask {
 public:
  AddSyncedVisitTask(const GURL& url, history::VisitRow visit)
      : url_(url), visit_(std::move(visit)) {}

  bool RunOnDBThread(history::HistoryBackend* backend,
                     history::HistoryDatabase* db) override {
    backend->AddSyncedVisit(url_, /*title=*/u"", /*hidden=*/false, visit_,
                            std::nullopt, std::nullopt);
    return true;
  }
  void DoneRunOnMainThread() override {}

 private:
  const GURL url_;
  const history::VisitRow visit_;
};

// Marks the visit at `visit_time` as known to sync, as Sync does once it has
// uploaded a local visit.
class MarkVisitAsKnownToSyncTask : public history::HistoryDBTask {
 public:
  explicit MarkVisitAsKnownToSyncTask(base::Time visit_time)
      : visit_time_(visit_time) {}

  bool RunOnDBThread(history::HistoryBackend* backend,
                     history::HistoryDatabase* db) override {
    history::VisitRow visit_row;
    if (backend->GetLastVisitByTime(visit_time_, &visit_row)) {
      static_cast<history::HistoryBackendForSync*>(backend)
          ->MarkVisitAsKnownToSync(visit_row.visit_id);
    }
    return true;
  }
  void DoneRunOnMainThread() override {}

 private:
  const base::Time visit_time_;
};

class RecentPagesCollectorBrowserTest : public InProcessBrowserTest {
 public:
  void SetUp() override {
    InitializeFeatureList();
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    ui_test_utils::WaitForHistoryToLoad(history_service());
  }

  virtual void InitializeFeatureList() {
    scoped_feature_list_.InitAndEnableFeature(
        kGlicBrowsingHistoryZeroStateSuggestions);
  }

 protected:
  history::HistoryService* history_service() {
    return HistoryServiceFactory::GetForProfile(
        browser()->GetProfile(), ServiceAccessType::EXPLICIT_ACCESS);
  }

  void AddVisit(const std::string& url,
                const std::u16string& title,
                base::Time visit_time,
                bool is_known_to_sync = true) {
    history_service()->AddPageWithDetails(
        GURL(url), title, /*visit_count=*/1, /*typed_count=*/0, visit_time,
        /*hidden=*/false, history::SOURCE_BROWSED);
    if (is_known_to_sync) {
      MarkVisitAsKnownToSync(visit_time);
    }
  }

  void MarkVisitAsKnownToSync(base::Time visit_time) {
    history_service()->ScheduleDBTask(
        FROM_HERE, std::make_unique<MarkVisitAsKnownToSyncTask>(visit_time),
        &task_tracker_);
  }

  Pages Collect(const std::vector<GURL>& page_context_urls = {}) {
    RecentPagesCollector collector(browser()->GetProfile());
    base::test::TestFuture<Pages> future;
    collector.CollectRecentPages(page_context_urls, future.GetCallback());
    return future.Take();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  base::CancelableTaskTracker task_tracker_;
};

IN_PROC_BROWSER_TEST_F(RecentPagesCollectorBrowserTest,
                       RecentPagesPopulatedAndFiltered) {
  // Set a third-party default search engine so its remote NTP is in use.
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(browser()->GetProfile());
  search_test_utils::WaitForTemplateURLServiceToLoad(template_url_service);
  TemplateURLData data;
  data.SetShortName(u"foo.com");
  data.SetURL("https://www.foo.com/url?bar={searchTerms}");
  data.new_tab_url = "https://www.foo.com/newtab";
  template_url_service->SetUserSelectedDefaultSearchProvider(
      template_url_service->Add(std::make_unique<TemplateURL>(data)));

  const base::Time now = base::Time::Now();
  // Pages already sent as page context (including with #ref) should be
  // excluded.
  AddVisit("https://www.focused.com/page#section", u"Focused",
           now - base::Minutes(1));
  // Non-HTTP(S) URLs and NTPs should be excluded.
  AddVisit("file:///tmp/local.html", u"Local File", now - base::Minutes(2));
  AddVisit("https://www.foo.com/newtab", u"New Tab", now - base::Minutes(3));
  // Visits older than kMaxRecentDuration (30m) should be excluded.
  AddVisit("https://www.oldvisit.com/page", u"Old Visit", now - base::Hours(1));
  // Blocked visits should be excluded.
  history::HistoryAddPageArgs blocked_args;
  blocked_args.url = GURL("https://www.blocked.com/page");
  blocked_args.time = now - base::Minutes(4);
  blocked_args.transition = ui::PAGE_TRANSITION_BLOCKED;
  history_service()->AddPage(blocked_args);
  MarkVisitAsKnownToSync(blocked_args.time);
  // Visits synced from other devices should be excluded.
  history::VisitRow foreign_visit;
  foreign_visit.visit_time = now - base::Minutes(5);
  foreign_visit.transition = ui::PAGE_TRANSITION_LINK;
  foreign_visit.originator_cache_guid = "other_device";
  foreign_visit.is_known_to_sync = true;
  history_service()->ScheduleDBTask(
      FROM_HERE,
      std::make_unique<AddSyncedVisitTask>(GURL("https://www.foreign.com/page"),
                                           foreign_visit),
      &task_tracker_);
  // Local visits not known to sync should be excluded.
  AddVisit("https://www.unsynced.com/page", u"Unsynced", now - base::Minutes(6),
           /*is_known_to_sync=*/false);

  // Eligible visits, including #ref deduplication keeping the most recent.
  AddVisit("https://www.history1.com/article", u"History 1",
           now - base::Minutes(20));
  AddVisit("https://www.history2.com/doc#part1", u"History 2 Old",
           now - base::Minutes(15));
  AddVisit("https://www.history2.com/doc#part2", u"History 2 New",
           now - base::Minutes(10));

  Pages pages = Collect({GURL("https://www.focused.com/page")});

  ASSERT_EQ(2u, pages.size());
  EXPECT_EQ("https://www.history2.com/doc", pages[0].url());
  EXPECT_EQ("History 2 New", pages[0].title());
  EXPECT_EQ((now - base::Minutes(10)).InMillisecondsSinceUnixEpoch(),
            pages[0].last_active_timestamp_ms());
  EXPECT_FALSE(pages[0].has_tab_id());

  EXPECT_EQ("https://www.history1.com/article", pages[1].url());
  EXPECT_EQ("History 1", pages[1].title());
  EXPECT_FALSE(pages[1].has_tab_id());
}

IN_PROC_BROWSER_TEST_F(RecentPagesCollectorBrowserTest,
                       MaxRecentPagesRespected) {
  const base::Time now = base::Time::Now();
  const int max_pages = kMaxRecentPages.Get();
  for (int i = 0; i < max_pages + 5; ++i) {
    AddVisit(base::StringPrintf("https://www.history.com/%d", i),
             base::UTF8ToUTF16(base::StringPrintf("History %d", i)),
             now - base::Seconds(i + 2));
  }
  // Ineligible and duplicate visits should not count towards the cap.
  AddVisit("file:///tmp/local.html", u"Local File", now);
  AddVisit("https://www.history.com/0#ref", u"History 0",
           now - base::Seconds(1));

  Pages pages = Collect();

  ASSERT_EQ(static_cast<size_t>(max_pages), pages.size());
  EXPECT_EQ("https://www.history.com/0", pages.front().url());
  EXPECT_EQ(base::StringPrintf("https://www.history.com/%d", max_pages - 1),
            pages.back().url());
}

IN_PROC_BROWSER_TEST_F(RecentPagesCollectorBrowserTest,
                       DoesNotRunCallbackAfterDestruction) {
  AddVisit("https://www.history1.com/article", u"History 1",
           base::Time::Now() - base::Minutes(1));

  bool called = false;
  auto collector =
      std::make_unique<RecentPagesCollector>(browser()->GetProfile());
  collector->CollectRecentPages(
      /*page_context_urls=*/{},
      base::BindOnce([](bool* called, Pages) { *called = true; }, &called));
  collector.reset();
  // Let the history reply reach the UI thread, then drain the UI thread.
  history::BlockUntilHistoryProcessesPendingRequests(history_service());
  base::RunLoop run_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_FALSE(called);
}

class RecentPagesCollectorOpenTabsBrowserTest
    : public RecentPagesCollectorBrowserTest {
 public:
  void InitializeFeatureList() override {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        kGlicBrowsingHistoryZeroStateSuggestions,
        {{kRecentPagesSource.name, "open_tabs_only"}});
  }

 protected:
  void OpenForegroundTab(BrowserWindowInterface* window,
                         const std::string& url) {
    ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
        window, GURL(url), WindowOpenDisposition::kNewForegroundTab,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  }
};

IN_PROC_BROWSER_TEST_F(RecentPagesCollectorOpenTabsBrowserTest,
                       OpenTabsPopulatedAcrossWindows) {
  OpenForegroundTab(browser(), "https://www.tab1.com/page");
  OpenForegroundTab(browser(), "https://www.focused.com/page");
  BrowserWindowInterface* second_browser =
      CreateBrowser(browser()->GetProfile());
  OpenForegroundTab(second_browser, "https://www.tab2.com/page");

  // The focused tab is already sent as page context and should be excluded.
  Pages pages = Collect({GURL("https://www.focused.com/page")});

  ASSERT_EQ(2u, pages.size());
  EXPECT_EQ("https://www.tab2.com/page", pages[0].url());
  EXPECT_EQ("https://www.tab1.com/page", pages[1].url());
}

// Tabs record base::Time::Now() when shown, so advancing the clock ages tabs
// that are already in the background.
base::Time OneHourFromNow() {
  return base::subtle::TimeNowIgnoringOverride() + base::Hours(1);
}

IN_PROC_BROWSER_TEST_F(RecentPagesCollectorOpenTabsBrowserTest,
                       OpenTabsOlderThanMaxRecentDurationExcluded) {
  OpenForegroundTab(browser(), "https://www.oldtab.com/page");

  base::subtle::ScopedTimeClockOverrides clock_override(
      &OneHourFromNow, /*time_ticks_override=*/nullptr,
      /*thread_ticks_override=*/nullptr);
  OpenForegroundTab(browser(), "https://www.newtab.com/page");

  Pages pages = Collect();

  ASSERT_EQ(1u, pages.size());
  EXPECT_EQ("https://www.newtab.com/page", pages[0].url());
}

}  // namespace
}  // namespace glic
