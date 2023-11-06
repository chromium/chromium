// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/closed_tab_cache.h"

#include "base/run_loop.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/sessions/closed_tab_cache_service_factory.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/sessions/tab_restore_service_load_waiter.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_tab_strip_model_delegate.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/memory_pressure/fake_memory_pressure_monitor.h"
#include "components/sessions/core/session_id.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"

using content::WebContents;

class ClosedTabCacheBrowserTest : public InProcessBrowserTest {
 public:
  ClosedTabCacheBrowserTest() = default;
  ~ClosedTabCacheBrowserTest() override = default;

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
  }

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    feature_list_.InitWithFeaturesAndParameters(
        {{features::kClosedTabCache, {}}, {kClosedTabCacheNoTimeEviction, {}}},
        {});

    InProcessBrowserTest::SetUpCommandLine(command_line);
  }

  // Add a tab to the given browser and navigate to url.
  void NavigateToURL(Browser* browser, const std::string& origin) {
    GURL server_url = embedded_test_server()->GetURL(origin, "/title1.html");
    ui_test_utils::NavigateToURLWithDisposition(
        browser, server_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  }

  void CloseTabAt(int index) {
    browser()->tab_strip_model()->CloseWebContentsAt(
        index, TabCloseTypes::CLOSE_CREATE_HISTORICAL_TAB);
  }

  void RestoreMostRecentTab() {
    TabRestoreServiceLoadWaiter waiter(
        TabRestoreServiceFactory::GetForProfile(browser()->profile()));
    chrome::RestoreTab(browser());
    waiter.Wait();
  }

  ClosedTabCache& closed_tab_cache() {
    return ClosedTabCacheServiceFactory::GetForProfile(browser()->profile())
        ->closed_tab_cache();
  }

  memory_pressure::test::FakeMemoryPressureMonitor fake_memory_pressure_monitor_;

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Add an entry to the cache when the cache is empty.
IN_PROC_BROWSER_TEST_F(ClosedTabCacheBrowserTest, StoreEntryWhenEmpty) {
  ASSERT_TRUE(embedded_test_server()->Start());

  NavigateToURL(browser(), "a.com");
  ASSERT_EQ(browser()->tab_strip_model()->count(), 2);

  ASSERT_TRUE(closed_tab_cache().IsEmpty())
      << "Expected cache to be empty at the start of the test.";
  CloseTabAt(1);
  ASSERT_EQ(browser()->tab_strip_model()->count(), 1);
  EXPECT_EQ(closed_tab_cache().EntriesCount(), 1U);
}

// Add an entry to the cache when there is enough space.
IN_PROC_BROWSER_TEST_F(ClosedTabCacheBrowserTest, StoreEntryBasic) {
  closed_tab_cache().SetCacheSizeLimitForTesting(2);

  ASSERT_TRUE(embedded_test_server()->Start());

  NavigateToURL(browser(), "a.com");
  NavigateToURL(browser(), "b.com");
  ASSERT_EQ(browser()->tab_strip_model()->count(), 3);

  ASSERT_TRUE(closed_tab_cache().IsEmpty())
      << "Expected cache to be empty at the start of the test.";

  CloseTabAt(1);
  EXPECT_EQ(closed_tab_cache().EntriesCount(), 1U);

  CloseTabAt(1);
  EXPECT_EQ(closed_tab_cache().EntriesCount(), 2U);
}

// Add an entry to the cache when the cache is at its limit.
IN_PROC_BROWSER_TEST_F(ClosedTabCacheBrowserTest, StoreEntryWhenFull) {
  ASSERT_TRUE(embedded_test_server()->Start());

  NavigateToURL(browser(), "a.com");
  NavigateToURL(browser(), "b.com");
  ASSERT_EQ(browser()->tab_strip_model()->count(), 3);

  ASSERT_TRUE(closed_tab_cache().IsEmpty())
      << "Expected cache to be empty at the start of the test.";

  CloseTabAt(1);
  EXPECT_EQ(closed_tab_cache().EntriesCount(), 1U);

  CloseTabAt(1);

  // Expect the cache size to still be 1.
  EXPECT_EQ(closed_tab_cache().EntriesCount(), 1U);
}

// Only add an entry to the cache when no beforeunload listeners are running.
IN_PROC_BROWSER_TEST_F(ClosedTabCacheBrowserTest,
                       StoreEntryWithoutBeforeunloadListener) {
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_TRUE(closed_tab_cache().IsEmpty())
      << "Expected cache to be empty at the start of the test.";

  // Don't cache WebContents when beforeunload listeners are run.
  NavigateToURL(browser(), "a.com");
  content::WebContents* a = browser()->tab_strip_model()->GetWebContentsAt(1);
  if (base::FeatureList::IsEnabled(
          blink::features::kBeforeunloadEventCancelByPreventDefault)) {
    EXPECT_TRUE(ExecJs(a->GetPrimaryMainFrame(),
                       "window.addEventListener('beforeunload', function (e) "
                       "{e.preventDefault();});"));
  } else {
    EXPECT_TRUE(ExecJs(a->GetPrimaryMainFrame(),
                       "window.addEventListener('beforeunload', function (e) "
                       "{e.returnValue = 'Not empty string';});"));
  }
  EXPECT_TRUE(a->NeedToFireBeforeUnloadOrUnloadEvents());
  CloseTabAt(1);
  EXPECT_EQ(closed_tab_cache().EntriesCount(), 0U);
  a->DispatchBeforeUnload(/*auto_cancel=*/false);

  // Cache WebContents when no beforeunload listeners are run.
  NavigateToURL(browser(), "b.com");
  content::WebContents* b = browser()->tab_strip_model()->GetWebContentsAt(2);
  EXPECT_FALSE(b->NeedToFireBeforeUnloadOrUnloadEvents());
  CloseTabAt(2);
  EXPECT_EQ(closed_tab_cache().EntriesCount(), 1U);

  // Ensure that the browser can shutdown. Otherwise tests might timeout.
  base::RepeatingCallback<void(bool)> on_close_confirmed = base::BindRepeating(
      [](bool placeholder) { LOG(ERROR) << "Should not be reached!"; });
  EXPECT_FALSE(browser()->TryToCloseWindow(/*skip_beforeunload=*/true,
                                           on_close_confirmed));
}

// Restore an entry when the cache is empty.
IN_PROC_BROWSER_TEST_F(ClosedTabCacheBrowserTest, RestoreEntryWhenEmpty) {
  ASSERT_TRUE(closed_tab_cache().IsEmpty())
      << "Expected cache to be empty at the start of the test.";

  SessionID id = SessionID::NewUnique();
  EXPECT_EQ(closed_tab_cache().RestoreEntry(id), nullptr);
}

// Restore an entry that is not in the cache.
IN_PROC_BROWSER_TEST_F(ClosedTabCacheBrowserTest, RestoreEntryWhenNotFound) {
  ASSERT_TRUE(embedded_test_server()->Start());

  NavigateToURL(browser(), "a.com");
  ASSERT_EQ(browser()->tab_strip_model()->count(), 2);

  ASSERT_TRUE(closed_tab_cache().IsEmpty())
      << "Expected cache to be empty at the start of the test.";

  CloseTabAt(1);
  EXPECT_EQ(closed_tab_cache().EntriesCount(), 1U);

  SessionID id = SessionID::NewUnique();
  EXPECT_EQ(closed_tab_cache().RestoreEntry(id), nullptr);
}

// Restore an entry that is in the cache.
IN_PROC_BROWSER_TEST_F(ClosedTabCacheBrowserTest, RestoreEntryWhenFound) {
  base::HistogramTester histogram_tester;
  const char kTabRestored[] = "Tab.RestoreClosedTab";

  ASSERT_TRUE(embedded_test_server()->Start());

  NavigateToURL(browser(), "a.com");
  ASSERT_EQ(browser()->tab_strip_model()->count(), 2);

  ASSERT_TRUE(closed_tab_cache().IsEmpty())
      << "Expected cache to be empty at the start of the test.";

  WebContents* wc = browser()->tab_strip_model()->GetWebContentsAt(1);
  CloseTabAt(1);
  EXPECT_EQ(closed_tab_cache().EntriesCount(), 1U);

  RestoreMostRecentTab();
  EXPECT_EQ(closed_tab_cache().EntriesCount(), 0U);
  ASSERT_EQ(browser()->tab_strip_model()->count(), 2);
  EXPECT_EQ(browser()->tab_strip_model()->GetWebContentsAt(1), wc);

  // We should store histogram kTabRestored when a tab is restored from
  // ClosedTabCache with value 1.
  EXPECT_EQ(histogram_tester.GetAllSamples(kTabRestored).size(), 1U);
  EXPECT_THAT(histogram_tester.GetAllSamples(kTabRestored),
              testing::ElementsAre(base::Bucket(1, 1)));
}

// TODO(crbug.com/1491942): This fails with the field trial testing config.
class ClosedTabCacheBrowserTestNoTestingConfig
    : public ClosedTabCacheBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ClosedTabCacheBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch("disable-field-trial-config");
  }
};

// Evict an entry after timeout.
IN_PROC_BROWSER_TEST_F(ClosedTabCacheBrowserTestNoTestingConfig,
                       EvictEntryOnTimeout) {
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner =
      base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  closed_tab_cache().SetTaskRunnerForTesting(task_runner);

  ASSERT_TRUE(embedded_test_server()->Start());

  NavigateToURL(browser(), "a.com");
  ASSERT_EQ(browser()->tab_strip_model()->count(), 2);

  ASSERT_TRUE(closed_tab_cache().IsEmpty())
      << "Expected cache to be empty at the start of the test.";

  CloseTabAt(1);
  EXPECT_EQ(closed_tab_cache().EntriesCount(), 1U);

  // Fast forward to just before eviction is due.
  base::TimeDelta delta = base::Milliseconds(1);
  base::TimeDelta ttl = ClosedTabCache::GetTimeToLiveInClosedTabCache();
  task_runner->FastForwardBy(ttl - delta);

  // Expect the entry to still be in the cache.
  EXPECT_EQ(closed_tab_cache().EntriesCount(), 1U);

  // Fast forward to when eviction is due.
  task_runner->FastForwardBy(delta);

  // Expect the entry to have been evicted.
  EXPECT_EQ(closed_tab_cache().EntriesCount(), 0U);
}

// Test for functionality of memory pressure in closed tab cache.
class ClosedTabCacheBrowserTestWithMemoryPressure
    : public ClosedTabCacheBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ClosedTabCacheBrowserTest::SetUpCommandLine(command_line);

    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{kClosedTabCacheMemoryPressure, {}}}, {});
  }

  void SetMemoryPressure(
      memory_pressure::test::FakeMemoryPressureMonitor::MemoryPressureLevel level) {
    fake_memory_pressure_monitor_.SetAndNotifyMemoryPressure(level);

    // Wait for all the pressure callbacks to be run.
    base::RunLoop().RunUntilIdle();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Check that no WebContents reaches the cache if the memory pressure level is
// critical and the threshold is critical.
IN_PROC_BROWSER_TEST_F(ClosedTabCacheBrowserTestWithMemoryPressure,
                       MemoryPressureLevelCritical) {
  ASSERT_TRUE(embedded_test_server()->Start());

  NavigateToURL(browser(), "a.com");
  ASSERT_EQ(browser()->tab_strip_model()->count(), 2);

  ASSERT_TRUE(closed_tab_cache().IsEmpty())
      << "Expected cache to be empty at the start of the test.";

  SetMemoryPressure(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);

  CloseTabAt(1);
  EXPECT_EQ(closed_tab_cache().EntriesCount(), 0U);
}

// Check that the cache is not cleared if the memory pressure level is moderate
// and the threshold is critical.
IN_PROC_BROWSER_TEST_F(ClosedTabCacheBrowserTestWithMemoryPressure,
                       MemoryPressureLevelModerate) {
  ASSERT_TRUE(embedded_test_server()->Start());

  NavigateToURL(browser(), "a.com");
  ASSERT_EQ(browser()->tab_strip_model()->count(), 2);

  ASSERT_TRUE(closed_tab_cache().IsEmpty())
      << "Expected cache to be empty at the start of the test.";

  CloseTabAt(1);
  EXPECT_EQ(closed_tab_cache().EntriesCount(), 1U);

  SetMemoryPressure(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE);

  // Expect the cache to not have been cleared since the memory pressure level
  // is below the threshold.
  EXPECT_EQ(closed_tab_cache().EntriesCount(), 1U);
}

// Check that a WebContents reaches the cache if the memory pressure level is
// critical and the threshold is moderate, but gets flushed once the threshold
// reaches critical.
IN_PROC_BROWSER_TEST_F(ClosedTabCacheBrowserTestWithMemoryPressure,
                       MemoryPressureLevelModerateThenCritical) {
  ASSERT_TRUE(embedded_test_server()->Start());

  NavigateToURL(browser(), "a.com");
  ASSERT_EQ(browser()->tab_strip_model()->count(), 2);

  ASSERT_TRUE(closed_tab_cache().IsEmpty())
      << "Expected cache to be empty at the start of the test.";

  SetMemoryPressure(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE);

  CloseTabAt(1);
  EXPECT_EQ(closed_tab_cache().EntriesCount(), 1U);

  SetMemoryPressure(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);

  // Expect the cache to have been cleared since the memory pressure level is
  // at the threshold.
  EXPECT_EQ(closed_tab_cache().EntriesCount(), 0U);
}
