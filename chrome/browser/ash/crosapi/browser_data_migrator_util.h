// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_BROWSER_DATA_MIGRATOR_UTIL_H_
#define CHROME_BROWSER_ASH_CROSAPI_BROWSER_DATA_MIGRATOR_UTIL_H_

#include <atomic>
#include <map>
#include <string>

#include "base/files/file_path.h"
#include "base/synchronization/atomic_flag.h"
#include "chrome/browser/ash/crosapi/migration_progress_tracker.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/leveldatabase/env_chromium.h"

namespace base {
class FilePath;
}

namespace ash::browser_data_migrator_util {

// User data directory name for lacros.
constexpr char kLacrosDir[] = "lacros";

// Profile data directory name for lacros.
constexpr char kLacrosProfilePath[] = "Default";

// The name of temporary directory that will store copies of files from the
// original user data directory. At the end of the migration, it will be moved
// to the appropriate destination.
constexpr char kTmpDir[] = "browser_data_migrator";

// `MoveMigrator` migrates user data to this directory first then moves it to
// the correct location as its final step.
constexpr char kMoveTmpDir[] = "move_migrator";

// `MoveMigrator` splits user data that needs to remain Ash into this directory
// first, then moves it to the correct location as its final step.
constexpr char kSplitTmpDir[] = "move_migrator_split";

// Directory for `MoveMigrator` to move hard links for lacros file/dirs in ash
// directory so that they become inaccessible from ash. This directory should be
// cleaned up after the migraton.
constexpr char kRemoveDir[] = "move_migrator_trash";

// The following UMAs are recorded from
// `DryRunToCollectUMA()`.
constexpr char kDryRunNoCopyDataSize[] =
    "Ash.BrowserDataMigrator.DryRunNoCopyDataSizeMB";
constexpr char kDryRunAshDataSize[] =
    "Ash.BrowserDataMigrator.DryRunAshDataSizeMB";
constexpr char kDryRunLacrosDataSize[] =
    "Ash.BrowserDataMigrator.DryRunLacrosDataSizeMB";
constexpr char kDryRunCommonDataSize[] =
    "Ash.BrowserDataMigrator.DryRunCommonDataSizeMB";
constexpr char kDryRunCopyMigrationTotalCopySize[] =
    "Ash.BrowserDataMigrator.DryRunTotalCopySizeMB.Copy";
constexpr char kDryRunMoveMigrationTotalCopySize[] =
    "Ash.BrowserDataMigrator.DryRunTotalCopySizeMB.Move";
constexpr char kDryRunMoveMigrationExtraSpaceReserved[] =
    "Ash.BrowserDataMigrator.DryRunExtraSizeReservedMB.Move";
constexpr char kDryRunMoveMigrationExtraSpaceRequired[] =
    "Ash.BrowserDataMigrator.DryRunExtraSizeRequiredMB.Move";

constexpr char kDryRunCopyMigrationHasEnoughDiskSpace[] =
    "Ash.BrowserDataMigrator.DryRunHasEnoughDiskSpace.Copy";
constexpr char kDryRunMoveMigrationHasEnoughDiskSpace[] =
    "Ash.BrowserDataMigrator.DryRunHasEnoughDiskSpace.Move";
constexpr char kDryRunDeleteAndCopyMigrationHasEnoughDiskSpace[] =
    "Ash.BrowserDataMigrator.DryRunHasEnoughDiskSpace.DeleteAndCopy";
constexpr char kDryRunDeleteAndMoveMigrationHasEnoughDiskSpace[] =
    "Ash.BrowserDataMigrator.DryRunHasEnoughDiskSpace.DeleteAndMove";

// The base names of files/dirs directly under the original profile
// data directory that can be deleted if needed because they are temporary
// storages.
constexpr const char* const kDeletablePaths[] = {
    kTmpDir,
    kMoveTmpDir,
    "blob_storage",
    "Cache",
    "Code Cache",
    "crash",
    "data_reduction_proxy_leveldb",
    "Download Service",
    "GCache",
    "heavy_ad_intervention_opt_out.db",
    "Network Action Predictor",
    "Network Persistent State",
    "optimization_guide_hint_cache_store",
    "previews_opt_out.db",
    "Reporting and NEL",
    "Site Characteristics Database",
    "TransportSecurity"};

// The base names of files/dirs that should remain in ash data
// directory.
constexpr const char* const kRemainInAshDataPaths[] = {
    "AccountManagerTokens.bin",
    "Accounts",
    "app_ranker.pb",
    "arc.apps",
    "autobrightness",
    "BudgetDatabase",
    "crostini.icons",
    "Downloads",
    "extension_install_log",
    "FullRestoreData",
    "GCM Store",
    "google-assistant-library",
    "GPUCache",
    "login-times",
    "logout-times",
    "MyFiles",
    "NearbySharePublicCertificateDatabase",
    "PPDCache",
    "PreferredApps",
    "PreferredApps",
    "PrintJobDatabase",
    "README",
    "RLZ Data",
    "smartcharging",
    "structured_metrics",
    "Translate Ranker Model",
    "Trusted Vault",
    "WebRTC Logs",
    "webrtc_event_logs",
    "zero_state_group_ranker.pb",
    "zero_state_local_files.pb"};

// The base names of files/dirs that are required for browsing and should be
// moved to lacros data dir.
constexpr const char* const kLacrosDataPaths[]{"AutofillStrikeDatabase",
                                               "Bookmarks",
                                               "Cookies",
                                               "databases",
                                               "DNR Extension Rules",
                                               "Extension Cookies",
                                               "Extension Rules",
                                               "Extension Scripts",
                                               "Extension State",
                                               "Extensions",
                                               "Favicons",
                                               "File System",
                                               "History",
                                               "IndexedDB",
                                               "Local App Settings",
                                               "Local Extension Settings",
                                               "Local Storage",
                                               "Managed Extension Settings",
                                               "QuotaManager",
                                               "Service Worker",
                                               "Session Storage",
                                               "Sessions",
                                               "Shortcuts",
                                               "Sync App Settings",
                                               "Top Sites",
                                               "Visited Links",
                                               "Web Applications",
                                               "Web Data"};

// The base names of files/dirs that are required by both ash and lacros and
// thus should be copied to lacros while keeping the original files/dirs in ash
// data dir.
constexpr const char* const kNeedCopyDataPaths[]{"Affiliation Database",
                                                 "Login Data",
                                                 "Platform Notifications",
                                                 "Policy",
                                                 "Preferences",
                                                 "shared_proto_db"};

// List of extension ids to be kept in Ash.
// TODO(crbug.com/1302613): fill this in with the complete list.
constexpr const char* const kExtensionKeepList[] = {
    "honijodknafkokifofgiaalefdiedpko",  // Help App
    "lfboplenmmjcmpbkeemecobbadnmpfhi",  // gnubbyd-v3
};

// Extensions path.
constexpr char kExtensionsFilePath[] = "Extensions";

// `Local Storage` paths.
constexpr char kLocalStorageFilePath[] = "Local Storage";
constexpr char kLocalStorageLeveldbName[] = "leveldb";

// State Store paths.
constexpr const char* const kStateStorePaths[] = {
    "Extension Rules",
    "Extension Scripts",
    "Extension State",
};

// The type of LevelDB schema.
enum class LevelDBType {
  kLocalStorage = 0,
  kStateStore = 1,
};

// Map from ExtensionID -> { leveldb keys..}.
using ExtensionKeys = std::map<std::string, std::vector<std::string>>;

constexpr char kTotalSize[] = "Ash.UserDataStatsRecorder.DataSize.TotalSize";

// UMA name prefix to record sizes of files/dirs in profile data directory. The
// name unique to each file/dir is appended to the end to create a full UMA name
// as follows `Ash.UserDataStatsRecorder.DataSize.{ItemName}`.
constexpr char kUserDataStatsRecorderDataSize[] =
    "Ash.UserDataStatsRecorder.DataSize.";

// Files/dirs that is not assigned a unique uma name is given this name.
constexpr char kUnknownUMAName[] = "Unknown";

constexpr int64_t kBytesInOneMB = 1024 * 1024;

// The size of disk space that should be kept free after migration. This is
// important since crypotohome conducts an aggressive disk cleanup if free disk
// space becomes less than 768MB. The buffer is rounded up to 1GB.
constexpr uint64_t kBuffer = 1024LL * 1024 * 1024;

// CancelFlag
class CancelFlag : public base::RefCountedThreadSafe<CancelFlag> {
 public:
  CancelFlag();
  CancelFlag(const CancelFlag&) = delete;
  CancelFlag& operator=(const CancelFlag&) = delete;

  void Set() { cancelled_ = true; }
  bool IsSet() const { return cancelled_; }

 private:
  friend base::RefCountedThreadSafe<CancelFlag>;

  ~CancelFlag();
  std::atomic_bool cancelled_;
};

// This is used to describe top level entries inside ash-chrome's profile data
// directory.
struct TargetItem {
  enum class ItemType { kFile, kDirectory };
  TargetItem(base::FilePath path, int64_t size, ItemType item_type);
  ~TargetItem() = default;
  bool operator==(const TargetItem& rhs) const;

  base::FilePath path;
  // The size of the TargetItem. If TargetItem is a directory, it is the sum
  // of all files under the directory.
  int64_t size;
  bool is_directory;
};

// `TargetItems` should hold `TargetItem`s of the same `ItemType`.
struct TargetItems {
  TargetItems();
  ~TargetItems();
  TargetItems(TargetItems&&);

  std::vector<TargetItem> items;
  // The sum of the sizes of `TargetItem`s in `items`.
  int64_t total_size;
};

// Specifies the type of `TargetItem`
enum class ItemType {
  kLacros = 0,       // Item that should be moved to lacros profile directory.
  kRemainInAsh = 1,  // Item that should remain in ash.
  kNeedCopy = 2,     // Item that should be copied to lacros.
  kDeletable = 3,    // Item that can be deleted to free up space i.e. cache.
};

// It enumerates the file/dirs in the given directory and returns items of
// `type`. E.g. `GetTargetItems(path, ItemType::kLacros)` will get all items
// that should be moved to lacros.
TargetItems GetTargetItems(const base::FilePath& original_profile_dir,
                           const ItemType type);

// Checks if there is enough disk space to migration to be carried out safely.
// that needs to be copied.
bool HasEnoughDiskSpace(const int64_t total_copy_size,
                        const base::FilePath& original_profile_dir);

// Returns extra bytes that has to be freed for the migration to be carried out
// if there are `total_copy_size` bytes of copying to be done. Returns 0 if no
// extra space needs to be freed.
uint64_t ExtraBytesRequiredToBeFreed(
    const int64_t total_copy_size,
    const base::FilePath& original_profile_dir);

// Injects the bytes to be returned by ExtraBytesRequiredToBeFreed above
// in RAII manner.
class ScopedExtraBytesRequiredToBeFreedForTesting {
 public:
  explicit ScopedExtraBytesRequiredToBeFreedForTesting(uint64_t bytes);
  ~ScopedExtraBytesRequiredToBeFreedForTesting();
};

// Copies `items` to `to_dir`.
bool CopyTargetItems(const base::FilePath& to_dir,
                     const TargetItems& items,
                     CancelFlag* cancel_flag,
                     MigrationProgressTracker* progress_tracker);

// Copies `item` to location pointed by `dest`. Returns true on success and
// false on failure.
bool CopyTargetItem(const TargetItem& item,
                    const base::FilePath& dest,
                    CancelFlag* cancel_flag,
                    MigrationProgressTracker* progress_tracker);

// Copies the contents of `from_path` to `to_path` recursively. Unlike
// `base::CopyDirectory()` it skips symlinks.
bool CopyDirectory(const base::FilePath& from_path,
                   const base::FilePath& to_path,
                   CancelFlag* cancel_flag,
                   MigrationProgressTracker* progress_tracker);

// Creates a hard link from `from_file` to `to_file`. Use it on a file and not a
// directory. Any parent directory of `to_file` should already exist. This will
// fail if `to_dir` already exists.
bool CreateHardLink(const base::FilePath& from_file,
                    const base::FilePath& to_file);

// Copies the content of `from_dir` to `to_dir` recursively similar to
// `CopyDirectory` while skipping symlinks. Unlike `CopyDirectory` it creates
// hard links for the files from `from_dir` to `to_dir`. If `to_dir`
// already exists, then this will fail.
bool CopyDirectoryByHardLinks(const base::FilePath& from_dir,
                              const base::FilePath& to_dir);

// Copies `items` to `to_dir` by calling `CreateHardLink()` for files and
// `CopyDirectoryBeHardLinks()` for directories.
bool CopyTargetItemsByHardLinks(const base::FilePath& to_dir,
                                const TargetItems& items,
                                CancelFlag* cancel_flag);

// Records the sizes of `TargetItem`s.
void RecordTargetItemSizes(const std::vector<TargetItem>& items);

// Records `size` of the file/dir pointed by `path`. If it is a directory, the
// size is the recursively accumulated sizes of contents inside.
void RecordUserDataSize(const base::FilePath& path, int64_t size);

// Collects migration specific UMAs without actually running the migration. It
// does not check if lacros is enabled.
void DryRunToCollectUMA(const base::FilePath& profile_data_dir);

// Given a leveldb instance and its type, output a map from
// ExtensionID -> { keys associated with the extension... }.
leveldb::Status GetExtensionKeys(leveldb::DB* db,
                                 LevelDBType leveldb_type,
                                 ExtensionKeys* result);

// Returns UMA name for `path`. Returns `kUnknownUMAName` if `path` is not in
// `kPathNamePairs`.
std::string GetUMAItemName(const base::FilePath& path);

// Similar to `base::ComputeDirectorySize()` this computes the sum of all files
// under `dir_path` recursively while skipping symlinks.
int64_t ComputeDirectorySizeWithoutLinks(const base::FilePath& dir_path);

// Record the total size of the user's profile data directory in MB.
void RecordTotalSize(int64_t size);

// Migrate the LevelDB instance at `original_path` to `target_path`,
// Filter out all the extensions that are not in `kExtensionKeepList`.
// `leveldb_type` determines the schema type.
bool MigrateLevelDB(const base::FilePath& original_path,
                    const base::FilePath& target_path,
                    const LevelDBType leveldb_type);

}  // namespace ash::browser_data_migrator_util

#endif  // CHROME_BROWSER_ASH_CROSAPI_BROWSER_DATA_MIGRATOR_UTIL_H_
