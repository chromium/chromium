// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_BROWSER_DATA_MIGRATOR_H_
#define CHROME_BROWSER_ASH_CROSAPI_BROWSER_DATA_MIGRATOR_H_

#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "chromeos/login/auth/user_context.h"

namespace ash {

// The new profile data directory location under the original profile data
// directory location. More concretely the new location will be
// `/home/chronos/u-<hash>/lacros/Default`.
constexpr char kLacrosProfileDir[] = "lacros/Default";

// BrowserDataMigrator is responsible for one time browser data migration from
// ash-chrome to lacros-chrome. The static method MaybeMigrate() instantiates
// an instance and calls MigrateInternal().
class BrowserDataMigrator {
 public:
  // Used to describe what files/dirs have to be migrated to the new location
  // and the total byte size of those files.
  struct TargetInfo {
    TargetInfo();
    ~TargetInfo();
    TargetInfo(const TargetInfo&);

    std::vector<base::FilePath> file_paths;
    std::vector<base::FilePath> dir_paths;
    int64_t total_byte_count;
  };

  // The class is instantiated on UI thread, bound to MigrateInternal and then
  // posted to worker thread.
  explicit BrowserDataMigrator(const base::FilePath& from);
  BrowserDataMigrator(const BrowserDataMigrator&) = delete;
  BrowserDataMigrator& operator=(const BrowserDataMigrator&) = delete;
  ~BrowserDataMigrator();

  // Called on UI thread. If lacros is enabled, it posts MigrateInternal() to a
  // worker thread with callback as reply. If lacros is not enabled, it calls
  // the callback immediately. Files are copied to |tmp_dir_| first and then
  // moved to |to_dir_| in an atomic way.
  static void MaybeMigrate(const UserContext& user_context,
                           base::OnceClosure callback);

 private:
  FRIEND_TEST_ALL_PREFIXES(BrowserDataMigratorTest, IsMigrationRequiredOnUI);
  FRIEND_TEST_ALL_PREFIXES(BrowserDataMigratorTest,
                           IsMigrationRequiredOnWorker);
  FRIEND_TEST_ALL_PREFIXES(BrowserDataMigratorTest, GetTargetInfo);
  FRIEND_TEST_ALL_PREFIXES(BrowserDataMigratorTest, Migrate);

  // Handles the migration on a worker thread. Returns whether a migration
  // occurred.
  bool MigrateInternal();
  // Called when the migration is finished on the UI thread.
  static void MigrateInternalFinishedUIThread(base::OnceClosure callback,
                                              bool did_migrate);
  // Checks if migration should happen. Called on UI thread.
  static bool IsMigrationRequiredOnUI(const user_manager::User* user);
  // Checks if migration should happen. Called on worker thread.
  bool IsMigrationRequiredOnWorker() const;
  // Gets what files/dirs need to be copied and the total byte size of files to
  // be copied.
  TargetInfo GetTargetInfo() const;
  // Compares space available under |from_dir_| against total byte size that
  // needs to be copied.
  bool HasEnoughDiskSpace(const TargetInfo& target_info) const;
  // Copies files from |from_dir_| to |tmp_dir_|.
  bool CopyToTmpDir(const TargetInfo& target_info) const;
  // Moves |tmp_dir_| to |to_dir_|.
  bool MoveTmpToTargetDir() const;

  // Path to the original profile data directory. It is directly under the
  // user data directory.
  base::FilePath from_dir_;
  // Path to the new profile data directory.
  base::FilePath to_dir_;
  // Path to temporary directory.
  base::FilePath tmp_dir_;
};

}  // namespace ash
#endif  // CHROME_BROWSER_ASH_CROSAPI_BROWSER_DATA_MIGRATOR_H_
