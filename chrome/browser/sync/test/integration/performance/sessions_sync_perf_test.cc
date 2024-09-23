// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/sync/test/integration/performance/sync_timing_helper.h"
#include "chrome/browser/sync/test/integration/sessions_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/test/browser_test.h"
#include "testing/perf/perf_result_reporter.h"

using content::OpenURLParams;
using sessions_helper::GetLocalSession;
using sessions_helper::GetSessionData;
using sessions_helper::OpenMultipleTabs;
using sessions_helper::SyncedSessionVector;
using sessions_helper::WaitForTabsToLoad;
using sync_timing_helper::TimeMutualSyncCycle;

static const int kNumTabs = 150;

namespace {

static constexpr char kMetricPrefixSession[] = "Session.";
static constexpr char kMetricAddTabSyncTime[] = "add_tab_sync_time";
static constexpr char kMetricUpdateTabSyncTime[] = "update_tab_sync_time";
static constexpr char kMetricDeleteTabSyncTime[] = "delete_tab_sync_time";

perf_test::PerfResultReporter SetUpReporter(const std::string& story) {
  perf_test::PerfResultReporter reporter(kMetricPrefixSession, story);
  reporter.RegisterImportantMetric(kMetricAddTabSyncTime, "ms");
  reporter.RegisterImportantMetric(kMetricUpdateTabSyncTime, "ms");
  reporter.RegisterImportantMetric(kMetricDeleteTabSyncTime, "ms");
  return reporter;
}

}  // namespace

class SessionsSyncPerfTest : public SyncTest {
 public:
  SessionsSyncPerfTest() : SyncTest(TWO_CLIENT) {}

  SessionsSyncPerfTest(const SessionsSyncPerfTest&) = delete;
  SessionsSyncPerfTest& operator=(const SessionsSyncPerfTest&) = delete;

  // Opens |num_tabs| new tabs on |profile|.
  void AddTabs(int profile, int num_tabs);

  // Update all tabs in |profile| by visiting a new URL.
  void UpdateTabs(int profile);

  // Returns the number of open tabs in all sessions (local + foreign) for
  // |profile|.  Returns -1 on failure.
  int GetTabCount(int profile);

 private:
  // Returns a new unique URL.
  GURL NextURL();

  // Returns a unique URL according to the integer |n|.
  GURL IntToURL(int n);

  int url_number_ = 0;
};

void SessionsSyncPerfTest::AddTabs(int profile, int num_tabs) {
  std::vector<GURL> urls;
  for (int i = 0; i < num_tabs; ++i) {
    urls.push_back(NextURL());
  }
  OpenMultipleTabs(profile, urls);
}

void SessionsSyncPerfTest::UpdateTabs(int profile) {
  Browser* browser = GetBrowser(profile);
  GURL url;
  std::vector<GURL> urls;
  for (int i = 0; i < browser->tab_strip_model()->count(); ++i) {
    chrome::SelectNumberedTab(browser, i);
    url = NextURL();
    browser->OpenURL(
        OpenURLParams(
            url,
            content::Referrer(GURL("http://localhost"),
                              network::mojom::ReferrerPolicy::kDefault),
            WindowOpenDisposition::CURRENT_TAB, ui::PAGE_TRANSITION_LINK,
            false),
        /*navigation_handle_callback=*/{});
    urls.push_back(url);
  }
  WaitForTabsToLoad(profile, urls);
}

int SessionsSyncPerfTest::GetTabCount(int profile) {
  const sync_sessions::SyncedSession* local_session;
  if (!GetLocalSession(profile, &local_session)) {
    DVLOG(1) << "GetLocalSession returned false";
    return -1;
  }

  SyncedSessionVector sessions;
  if (!GetSessionData(profile, &sessions)) {
    // Foreign session data may be empty.  In this case we only count tabs in
    // the local session.
    DVLOG(1) << "GetSessionData returned false";
  }

  int tab_count = 0;
  sessions.push_back(local_session);
  for (const sync_sessions::SyncedSession* session : sessions) {
    for (const auto& [window_id, window] : session->windows) {
      tab_count += window->wrapped_window.tabs.size();
    }
  }

  return tab_count;
}

GURL SessionsSyncPerfTest::NextURL() {
  return IntToURL(url_number_++);
}

GURL SessionsSyncPerfTest::IntToURL(int n) {
  return GURL(base::StringPrintf("http://localhost/%d", n));
}

IN_PROC_BROWSER_TEST_F(SessionsSyncPerfTest, P0) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  perf_test::PerfResultReporter reporter =
      SetUpReporter(base::NumberToString(kNumTabs) + "_tabs");
  AddTabs(0, kNumTabs);
  base::TimeDelta dt = TimeMutualSyncCycle(GetClient(0), GetClient(1));
  ASSERT_EQ(kNumTabs, GetTabCount(0));
  ASSERT_EQ(kNumTabs, GetTabCount(1));
  reporter.AddResult(kMetricAddTabSyncTime, dt);

  UpdateTabs(0);
  dt = TimeMutualSyncCycle(GetClient(0), GetClient(1));
  ASSERT_EQ(kNumTabs, GetTabCount(0));
  ASSERT_EQ(kNumTabs, GetTabCount(1));
  reporter.AddResult(kMetricUpdateTabSyncTime, dt);
}
