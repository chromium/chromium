// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <string_view>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/resource_coordinator/tab_load_tracker_test_support.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/services/storage/dom_storage/dom_storage_database.h"
#include "components/services/storage/dom_storage/features.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/dom_storage_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/storage_usage_info.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr const char kKey[] = "test_key";
constexpr const char kValue[] = "test_value";

bool IsOutermostPreTest() {
  return std::string_view(
             testing::UnitTest::GetInstance()->current_test_info()->name())
      .starts_with("PRE_PRE_");
}

std::string ToJavaScriptName(storage::StorageType storage_type) {
  switch (storage_type) {
    case storage::StorageType::kLocalStorage:
      return "localStorage";
    case storage::StorageType::kSessionStorage:
      return "sessionStorage";
  }
  NOTREACHED();
}

std::string ToTestName(storage::StorageType storage_type) {
  switch (storage_type) {
    case storage::StorageType::kLocalStorage:
      return "LocalStorage";
    case storage::StorageType::kSessionStorage:
      return "SessionStorage";
  }
  NOTREACHED();
}

}  // namespace

// Exercises LevelDB to SQLite migration end-to-end.
//
// On desktop, the storage service runs out-of-process and sand boxed. The
// migration performs several on-disk operations, including existence checks,
// staging a SQLite database, moving it into place, and deleting the old
// LevelDB.  A silent sandbox failure in any of these is transparent to the
// in-process, unsandboxed unit tests, making a browser test necessary.
//
// TODO(crbug.com/377242771): Add coverage for discarding stale `*Migrating`
// files, shutting down or recreating the database during migration, replaying
// all queued operations, and enabling migration together with
// `kDomStorageSqliteNewDatabases`.
class DOMStorageMigrationBrowserTest
    : public testing::WithParamInterface<storage::StorageType>,
      public InProcessBrowserTest {
 public:
  DOMStorageMigrationBrowserTest() {
    if (IsOutermostPreTest()) {
      // The first run creates a LevelDB with migration disabled,
      // mimicking a profile from before the migration rollout.
      feature_list_.InitWithFeatures(
          /*enabled_features=*/{},
          /*disabled_features=*/{storage::kDomStorageSqliteMigration,
                                 storage::kDomStorageSqlite,
                                 storage::kDomStorageSqliteInMemory,
                                 storage::kDomStorageSqliteNewDatabases});
    } else {
      // The main run enables migration with a short idle timeout so the
      // migration triggers promptly once the database goes idle.
      feature_list_.InitAndEnableFeatureWithParameters(
          storage::kDomStorageSqliteMigration,
          {{storage::kDomStorageSqliteMigrationInactivityTimeoutParam.name,
            "100ms"}});
    }
  }

  void SetUpOnMainThread() override {
    // Enable browser session restore to persist session storage.
    SessionStartupPref::SetStartupPref(
        browser()->GetProfile(), SessionStartupPref(SessionStartupPref::LAST));
  }

  storage::StorageType storage_type() { return GetParam(); }

  GURL test_url() { return content::GetTestUrl(nullptr, "title1.html"); }

  // Blocks until session restore finishes.
  void WaitForTabsToLoad() {
    TabStripModel* tab_strip_model = browser()->GetTabStripModel();
    for (int i = 0; i < tab_strip_model->count(); ++i) {
      content::WebContents* contents = tab_strip_model->GetWebContentsAt(i);
      contents->GetController().LoadIfNecessary();
      resource_coordinator::WaitForTransitionToLoaded(contents);
    }
    // For each session restore, in addition to restoring the prior session,
    // browser tests also add a new tab at about:blank.  Ignore the new
    // about:blank tab by selecting the tab at index 0.
    ASSERT_GT(tab_strip_model->count(), 0);
    tab_strip_model->SelectTabAt(0);
    EXPECT_EQ(web_contents()->GetLastCommittedURL(), test_url());
  }

 protected:
  content::WebContents* web_contents() {
    return browser()->GetTabStripModel()->GetActiveWebContents();
  }

  content::StoragePartition* storage_partition() {
    return web_contents()->GetBrowserContext()->GetDefaultStoragePartition();
  }

  content::DOMStorageContext* dom_storage_context() {
    return storage_partition()->GetDOMStorageContext();
  }

  base::FilePath LevelDbDir(storage::StorageType storage_type) {
    return storage::DomStorageDatabase::GetLevelDbPath(
        storage_type, storage_partition()->GetPath());
  }

  base::FilePath SqliteDbPath(storage::StorageType storage_type) {
    return storage::DomStorageDatabase::GetSqlitePath(
        storage_type, storage_partition()->GetPath());
  }

  void WriteKeyValue(storage::StorageType storage_type,
                     const std::string& key,
                     const std::string& value) {
    ASSERT_TRUE(content::ExecJs(web_contents(), ToJavaScriptName(storage_type) +
                                                    ".setItem('" + key +
                                                    "', '" + value + "');"));
  }

  std::string ReadKeyValue(storage::StorageType storage_type,
                           const std::string& key) {
    return content::EvalJs(web_contents(), ToJavaScriptName(storage_type) +
                                               ".getItem('" + key + "')")
        .ExtractString();
  }

  // Read the local storage usage information from the database to ensure that
  // it is opened.
  void EnsureDatabaseOpened() {
    base::test::TestFuture<const std::vector<content::StorageUsageInfo>&>
        usage_future;
    dom_storage_context()->GetLocalStorageUsage(usage_future.GetCallback());
    ASSERT_TRUE(usage_future.Wait());
  }

  // Blocks until migration completes.  Checks migration status every 100ms.
  void WaitForMigration(const base::FilePath& leveldb_path,
                        const base::FilePath& sqlite_path) {
    // Migration should finish promptly with the 100ms idle timeout configured
    // above. `action_max_timeout()` guards against a hang.
    const base::TimeTicks hang_safety_net_deadline =
        base::TimeTicks::Now() + TestTimeouts::action_max_timeout();

    while (base::TimeTicks::Now() < hang_safety_net_deadline) {
      {
        base::ScopedAllowBlockingForTesting allow_blocking;
        if (base::PathExists(sqlite_path) && !base::PathExists(leveldb_path)) {
          return;
        }
      }
      base::RunLoop loop;
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE, loop.QuitClosure(), base::Milliseconds(100));
      loop.Run();
    }
    FAIL() << "Timed out waiting for LevelDB-to-SQLite migration";
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// With the migration feature disabled, writing to dom storage creates an
// on-disk LevelDB and no SQLite database. This initializes a pre-migration
// profile for the migration test.
IN_PROC_BROWSER_TEST_P(DOMStorageMigrationBrowserTest, PRE_PRE_Migration) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url()));

  ASSERT_NO_FATAL_FAILURE(EnsureDatabaseOpened());
  ASSERT_NO_FATAL_FAILURE(WriteKeyValue(storage_type(), kKey, kValue));

  // The LevelDB must exist.
  base::ScopedAllowBlockingForTesting allow_blocking;
  EXPECT_TRUE(base::PathExists(LevelDbDir(storage_type())));
  EXPECT_FALSE(base::PathExists(SqliteDbPath(storage_type())));
}

// With the migration feature enabled, the pre-existing LevelDB migrates to
// SQLite after the database becomes idle.  Migration removes the old LevelDB.
IN_PROC_BROWSER_TEST_P(DOMStorageMigrationBrowserTest, PRE_Migration) {
  ASSERT_NO_FATAL_FAILURE(WaitForTabsToLoad());

  // Open the pre-existing LevelDB, verifying its key/value pairs, then let
  // database become idle so migration runs.
  EXPECT_EQ(kValue, ReadKeyValue(storage_type(), kKey));

  ASSERT_NO_FATAL_FAILURE(WaitForMigration(LevelDbDir(storage_type()),
                                           SqliteDbPath(storage_type())));

  // After migration, a SQLite database must exist.
  base::ScopedAllowBlockingForTesting allow_blocking;
  EXPECT_TRUE(base::PathExists(SqliteDbPath(storage_type())));
  EXPECT_FALSE(base::PathExists(LevelDbDir(storage_type())));
}

IN_PROC_BROWSER_TEST_P(DOMStorageMigrationBrowserTest, Migration) {
  ASSERT_NO_FATAL_FAILURE(WaitForTabsToLoad());
  EXPECT_EQ(kValue, ReadKeyValue(storage_type(), kKey));

  // The migrated SQLite database must still exist.
  base::ScopedAllowBlockingForTesting allow_blocking;
  EXPECT_TRUE(base::PathExists(SqliteDbPath(storage_type())));
  EXPECT_FALSE(base::PathExists(LevelDbDir(storage_type())));
}

#if BUILDFLAG(IS_ANDROID)
// Android always deletes all session storage on startup, which prevents
// migration.  Only test local storage migration on Android.
INSTANTIATE_TEST_SUITE_P(
    /*no prefix*/,
    DOMStorageMigrationBrowserTest,
    ::testing::Values(storage::StorageType::kLocalStorage),
    /*name_generator=*/
    [](const testing::TestParamInfo<DOMStorageMigrationBrowserTest::ParamType>&
           info) { return ToTestName(info.param); });
#else
INSTANTIATE_TEST_SUITE_P(
    /*no prefix*/,
    DOMStorageMigrationBrowserTest,
    ::testing::Values(storage::StorageType::kLocalStorage,
                      storage::StorageType::kSessionStorage),
    /*name_generator=*/
    [](const testing::TestParamInfo<DOMStorageMigrationBrowserTest::ParamType>&
           info) { return ToTestName(info.param); });
#endif
