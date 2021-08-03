// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/closed_tab_cache.h"

#include "base/run_loop.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "base/util/memory_pressure/fake_memory_pressure_monitor.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/sessions/core/session_id.h"
#include "content/public/test/browser_test.h"

using content::WebContents;

class ClosedTabCacheTest : public InProcessBrowserTest {
 public:
  ClosedTabCacheTest() = default;
  ClosedTabCacheTest(const ClosedTabCacheTest&) = delete;
  ClosedTabCacheTest& operator=(const ClosedTabCacheTest&) = delete;

 protected:
  // Add a tab to the given browser.
  void AddTab(Browser* browser) {
    ui_test_utils::NavigateToURLWithDisposition(
        browser, GURL(chrome::kChromeUINewTabURL),
        WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  }

  util::test::FakeMemoryPressureMonitor fake_memory_pressure_monitor_;
};

// Add an entry to the cache when the cache is empty.
IN_PROC_BROWSER_TEST_F(ClosedTabCacheTest, StoreEntryWhenEmpty) {
  ClosedTabCache cache;

  AddTab(browser());
  ASSERT_EQ(browser()->tab_strip_model()->count(), 2);

  std::unique_ptr<WebContents> wc =
      browser()->tab_strip_model()->DetachWebContentsAtForInsertion(0);

  ASSERT_TRUE(cache.IsEmpty())
      << "Expected cache to be empty at the start of the test.";

  cache.StoreEntry(SessionID::NewUnique(), std::move(wc),
                   base::TimeTicks::Now());
  EXPECT_EQ(cache.EntriesCount(), 1U);
}

// Add an entry to the cache when there is enough space.
IN_PROC_BROWSER_TEST_F(ClosedTabCacheTest, StoreEntryBasic) {
  ClosedTabCache cache;

  cache.SetCacheSizeLimitForTesting(2);

  AddTab(browser());
  AddTab(browser());
  ASSERT_EQ(browser()->tab_strip_model()->count(), 3);

  std::unique_ptr<WebContents> wc1 =
      browser()->tab_strip_model()->DetachWebContentsAtForInsertion(0);
  std::unique_ptr<WebContents> wc2 =
      browser()->tab_strip_model()->DetachWebContentsAtForInsertion(0);

  ASSERT_TRUE(cache.IsEmpty())
      << "Expected cache to be empty at the start of the test.";

  cache.StoreEntry(SessionID::NewUnique(), std::move(wc1),
                   base::TimeTicks::Now());
  EXPECT_EQ(cache.EntriesCount(), 1U);

  cache.StoreEntry(SessionID::NewUnique(), std::move(wc2),
                   base::TimeTicks::Now());
  EXPECT_EQ(cache.EntriesCount(), 2U);
}

// Add an entry to the cache when the cache is at its limit.
IN_PROC_BROWSER_TEST_F(ClosedTabCacheTest, StoreEntryWhenFull) {
  ClosedTabCache cache;

  AddTab(browser());
  AddTab(browser());
  ASSERT_EQ(browser()->tab_strip_model()->count(), 3);

  std::unique_ptr<WebContents> wc1 =
      browser()->tab_strip_model()->DetachWebContentsAtForInsertion(0);
  std::unique_ptr<WebContents> wc2 =
      browser()->tab_strip_model()->DetachWebContentsAtForInsertion(0);
  SessionID id1 = SessionID::NewUnique();

  ASSERT_TRUE(cache.IsEmpty())
      << "Expected cache to be empty at the start of the test.";

  cache.StoreEntry(id1, std::move(wc1), base::TimeTicks::Now());
  EXPECT_EQ(cache.EntriesCount(), 1U);

  cache.StoreEntry(SessionID::NewUnique(), std::move(wc2),
                   base::TimeTicks::Now());

  // Expect the cache size to still be 1 and the removed entry to be entry1.
  EXPECT_EQ(cache.EntriesCount(), 1U);
  EXPECT_EQ(cache.GetWebContents(id1), nullptr);
}

// Restore an entry when the cache is empty.
IN_PROC_BROWSER_TEST_F(ClosedTabCacheTest, RestoreEntryWhenEmpty) {
  ClosedTabCache cache;

  ASSERT_TRUE(cache.IsEmpty())
      << "Expected cache to be empty at the start of the test.";

  SessionID id = SessionID::NewUnique();
  EXPECT_EQ(cache.RestoreEntry(id), nullptr);
}

// Restore an entry that is not in the cache.
IN_PROC_BROWSER_TEST_F(ClosedTabCacheTest, RestoreEntryWhenNotFound) {
  ClosedTabCache cache;

  AddTab(browser());
  ASSERT_EQ(browser()->tab_strip_model()->count(), 2);

  std::unique_ptr<WebContents> wc =
      browser()->tab_strip_model()->DetachWebContentsAtForInsertion(0);

  ASSERT_TRUE(cache.IsEmpty())
      << "Expected cache to be empty at the start of the test.";

  cache.StoreEntry(SessionID::NewUnique(), std::move(wc),
                   base::TimeTicks::Now());
  EXPECT_EQ(cache.EntriesCount(), 1U);

  SessionID id = SessionID::NewUnique();
  EXPECT_EQ(cache.RestoreEntry(id), nullptr);
}

// Restore an entry that is in the cache.
IN_PROC_BROWSER_TEST_F(ClosedTabCacheTest, RestoreEntryWhenFound) {
  ClosedTabCache cache;

  AddTab(browser());
  ASSERT_EQ(browser()->tab_strip_model()->count(), 2);

  std::unique_ptr<WebContents> wc =
      browser()->tab_strip_model()->DetachWebContentsAtForInsertion(0);

  ASSERT_TRUE(cache.IsEmpty())
      << "Expected cache to be empty at the start of the test.";

  SessionID id = SessionID::NewUnique();
  cache.StoreEntry(id, std::move(wc), base::TimeTicks::Now());
  EXPECT_EQ(cache.EntriesCount(), 1U);

  EXPECT_NE(cache.RestoreEntry(id), nullptr);
}

// Evict an entry after timeout.
IN_PROC_BROWSER_TEST_F(ClosedTabCacheTest, EvictEntryOnTimeout) {
  ClosedTabCache cache;

  scoped_refptr<base::TestMockTimeTaskRunner> task_runner =
      base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  cache.SetTaskRunnerForTesting(task_runner);

  AddTab(browser());
  ASSERT_EQ(browser()->tab_strip_model()->count(), 2);

  std::unique_ptr<WebContents> wc =
      browser()->tab_strip_model()->DetachWebContentsAtForInsertion(0);

  ASSERT_TRUE(cache.IsEmpty())
      << "Expected cache to be empty at the start of the test.";

  cache.StoreEntry(SessionID::NewUnique(), std::move(wc),
                   base::TimeTicks::Now());
  EXPECT_EQ(cache.EntriesCount(), 1U);

  // Fast forward to just before eviction is due.
  base::TimeDelta delta = base::TimeDelta::FromMilliseconds(1);
  base::TimeDelta ttl = ClosedTabCache::GetTimeToLiveInClosedTabCache();
  task_runner->FastForwardBy(ttl - delta);

  // Expect the entry to still be in the cache.
  EXPECT_EQ(cache.EntriesCount(), 1U);

  // Fast forward to when eviction is due.
  task_runner->FastForwardBy(delta);

  // Expect the entry to have been evicted.
  EXPECT_EQ(cache.EntriesCount(), 0U);
}

// Check that the cache is cleared if the memory pressure level is critical and
// the threshold is critical.
IN_PROC_BROWSER_TEST_F(ClosedTabCacheTest, MemoryPressureLevelCritical) {
  ClosedTabCache cache;

  AddTab(browser());
  ASSERT_EQ(browser()->tab_strip_model()->count(), 2);

  std::unique_ptr<WebContents> wc =
      browser()->tab_strip_model()->DetachWebContentsAtForInsertion(0);

  ASSERT_TRUE(cache.IsEmpty())
      << "Expected cache to be empty at the start of the test.";

  cache.StoreEntry(SessionID::NewUnique(), std::move(wc),
                   base::TimeTicks::Now());
  EXPECT_EQ(cache.EntriesCount(), 1U);

  fake_memory_pressure_monitor_.SetAndNotifyMemoryPressure(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);

  // Wait for all the pressure callbacks to be run.
  base::RunLoop().RunUntilIdle();

  // Expect the cache to have been cleared since the memory pressure level is
  // at the threshold.
  EXPECT_EQ(cache.EntriesCount(), 0U);
}

// Check that the cache is not cleared if the memory pressure level is moderate
// and the threshold is critical.
IN_PROC_BROWSER_TEST_F(ClosedTabCacheTest, MemoryPressureLevelModerate) {
  ClosedTabCache cache;

  AddTab(browser());
  ASSERT_EQ(browser()->tab_strip_model()->count(), 2);

  std::unique_ptr<WebContents> wc =
      browser()->tab_strip_model()->DetachWebContentsAtForInsertion(0);

  ASSERT_TRUE(cache.IsEmpty())
      << "Expected cache to be empty at the start of the test.";

  cache.StoreEntry(SessionID::NewUnique(), std::move(wc),
                   base::TimeTicks::Now());
  EXPECT_EQ(cache.EntriesCount(), 1U);

  fake_memory_pressure_monitor_.SetAndNotifyMemoryPressure(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE);
  base::RunLoop().RunUntilIdle();

  // Expect the cache to not have been cleared since the memory pressure level
  // is below the threshold.
  EXPECT_EQ(cache.EntriesCount(), 1U);
}
