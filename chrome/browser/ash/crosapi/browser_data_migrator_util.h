// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_BROWSER_DATA_MIGRATOR_UTIL_H_
#define CHROME_BROWSER_ASH_CROSAPI_BROWSER_DATA_MIGRATOR_UTIL_H_

#include <atomic>
#include <map>
#include <optional>
#include <string>
#include <string_view>

#include "base/files/file_path.h"
#include "base/synchronization/atomic_flag.h"
#include "base/values.h"
#include "chromeos/ash/components/standalone_browser/migration_progress_tracker.h"
#include "components/sync/base/data_type.h"
#include "third_party/leveldatabase/env_chromium.h"

namespace base {
class FilePath;
}

namespace ash {

namespace standalone_browser {
class MigrationProgressTracker;
}  // namespace standalone_browser

namespace browser_data_migrator_util {

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

constexpr char kDryRunExtraDiskSpaceOccupiedByMove[] =
    "Ash.BrowserDataMigrator.DryRunExtraDiskSpaceOccupiedByMove";
constexpr char kDryRunFreeDiskSpaceAfterDelete[] =
    "Ash.BrowserDataMigrator.DryRunFreeDiskSpaceAfterDelete";
constexpr char kDryRunFreeDiskSpaceAfterMigration[] =
    "Ash.BrowserDataMigrator.DryRunFreeDiskSpaceAfterMigration";

// Collect extra info for users with low disk space.
constexpr char kDryRunExtraDiskSpaceOccupiedByMoveLowDiskUser2[] =
    "Ash.BrowserDataMigrator.DryRunExtraDiskSpaceOccupiedByMove.LowDiskUser2";
constexpr char kDryRunFreeDiskSpaceLowDiskUser2[] =
    "Ash.BrowserDataMigrator.DryRunFreeDiskSpace.LowDiskUser2";
constexpr char kDryRunFreeDiskSpaceAfterDeleteLowDiskUser2[] =
    "Ash.BrowserDataMigrator.DryRunFreeDiskSpaceAfterDelete.LowDiskUser2";
constexpr char kDryRunProfileDirSizeLowDiskUser2[] =
    "Ash.BrowserDataMigrator.DryRunProfileDirSize.LowDiskUser2";
constexpr char kDryRunMyFilesDirSizeLowDiskUser2[] =
    "Ash.BrowserDataMigrator.DryRunMyFilesDirSize.LowDiskUser2";

// The base names of files/dirs directly under the original profile
// data directory that can be deleted if needed because they are temporary
// storages.
constexpr const char* const kDeletablePaths[] = {
    kTmpDir,
    kMoveTmpDir,
    "blob_storage",
    "Cache",
    "Code Cache",
    "coupon_db",
    "crash",
    "Download Service",
    "GPUCache",
    "heavy_ad_intervention_opt_out.db",
    "merchant_signal_db",
    "Network Action Predictor",
    "Network Persistent State",
    "previews_opt_out.db",
    "Reporting and NEL",
    "Site Characteristics Database",
    "Translate Ranker Model",
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
    "data_reduction_proxy_leveldb",
    "Downloads",
    "extension_install_log",
    "Feature Engagement Tracker",
    "FullRestoreData",
    "GCM Store",
    "google-assistant-library",
    "input_methods",
    "launcher_ranking",
    "LOCK",
    "LOG",
    "LOG.old",
    "login-times",
    "logout-times",
    "MyFiles",
    "NearbySharePublicCertificateDatabase",
    "PPDCache",
    "PreferredApps",
    "PrintJobDatabase",
    "README",
    "RLZ Data",
    "smartcharging",
    "structured_metrics",
    "Sync Data",
    "Trusted Vault",
    "trusted_vault.pb",
    "WebRTC Logs",
    "webrtc_event_logs",
    "zero_state_group_ranker.pb",
    "zero_state_local_files.pb"};

// The base names of files/dirs that are required for browsing and should be
// moved to lacros data dir.
constexpr const char* const kLacrosDataPaths[]{
    "Affiliation Database",
    "AutofillStrikeDatabase",
    "Bookmarks",
    "chrome_cart_db",
    "commerce_subscription_db",
    "Cookies",
    "databases",
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
    "Login Data",
    "Login Data For Account",
    "optimization_guide_hint_cache_store",
    "optimization_guide_model_and_features_store",
    "Managed Extension Settings",
    "persisted_state_db",
    "Platform Notifications",
    "QuotaManager",
    "Safe Browsing Cookies",
    "Safe Browsing Network",
    "Service Worker",
    "Session Storage",
    "Sessions",
    "SharedStorage",
    "Shortcuts",
    "Storage",
    "Sync App Settings",
    "Sync Extension Settings",
    "Top Sites",
    "Visited Links",
    "Web Applications",
    "Web Data",
    "WebStorage"};

// The base names of files/dirs that are required by both ash and lacros and
// thus should be copied to lacros while keeping the original files/dirs in ash
// data dir.
constexpr const char* const kNeedCopyForMoveDataPaths[]{
    "DNR Extension Rules", "Extension Cookies", "shared_proto_db"};

// The same as `kNeedCopyDataPathsForMove` + "Preferences".
constexpr const char* const kNeedCopyForCopyDataPaths[]{
    "DNR Extension Rules", "Extension Cookies", "Preferences",
    "shared_proto_db"};

// List of extension ids to be kept in Ash.
constexpr const char* const kExtensionsAshOnly[] = {
    "gjjabgpgjpampikjhjpfhneeoapjbjaf",  // Google Speech Synthesis Ext. (patts)
    "dakbfdmgjiabojdgbiljlhgjbokobjpg",  // ESpeak Speech Synthesis Extension
    "jacnkoglebceckolkoapelihnglgaicd",  // Enhanced Network Tts Extension
    "klbcgckkldhdhonijdbnhhaiedfkllef",  // Select to Speak Extension
    "egfdjlfmgnehecnclamagfafdccgfndp",  // Accessibility Common Extension
    "mndnfokpggljbaajbnioimlmbfngpief",  // Chrome Vox Extension
    "pmehocpgjmkenlokgjfkaichfjdhpeol",  // Switch Access Extension
    "jddehjeebkoimngcbdkaahpobgicbffp",  // Braille IME (in IME allowlist)
    "mppnpdlheglhdfmldimlhpnegondlapf",  // Keyboard App Extension
    "mecfefiddjlmabpeilblgegnbioikfmp",  // sign in profile testing extension
    "behllobkkfkfnphdnhnkndlbkcpglgmj",  // guest mode test extension
    "honijodknafkokifofgiaalefdiedpko",  // Help App
    "pmfjbimdmchhbnneeidfognadeopoehp",  // Image Loader Extension
    "cnbgggchhmkkdmeppjobngjoejnihlei",  // Arc Support (Play Store)
};

// Extensions path.
constexpr char kExtensionsFilePath[] = "Extensions";

// IndexedDB path.
constexpr char kIndexedDBFilePath[] = "IndexedDB";

// `Local Storage` paths.
constexpr char kLocalStorageFilePath[] = "Local Storage";
constexpr char kLocalStorageLeveldbName[] = "leveldb";

// `Sync Data` path.
constexpr char kSyncDataFilePath[] = "Sync Data";
constexpr char kSyncDataLeveldbName[] = "LevelDB";
constexpr char kSyncDataNigoriFileName[] = "Nigori.bin";

// State Store paths.
constexpr const char* const kStateStorePaths[] = {
    "Extension Rules",
    "Extension Scripts",
    "Extension State",
};

// `Storage` path.
constexpr char kStorageFilePath[] = "Storage";
constexpr char kStorageExtFilePath[] = "ext";

// Values used for the kBrowserDataMigrationMode flag.
constexpr char kCopySwitchValue[] =
    "copy";  // Corresponds to kCopy. No longer in use.
constexpr char kMoveSwitchValue[] = "move";  // Corresponds to KMove.

// Preference that indicates that sync setup has been completed at least once.
// Doesn't exist in Ash and need to be set explicitly during the migration.
// Exposed for testing.
constexpr char kSyncInitialSyncFeatureSetupCompletePrefName[] =
    "sync.has_setup_completed";

// The type of LevelDB schema.
enum class LevelDBType {
  kLocalStorage = 0,
  kStateStore = 1,
};

// Map from ExtensionID -> { leveldb keys..}.
using ExtensionKeys = std::map<std::string, std::vector<std::string>>;

// Structure containing both IndexedDB paths for an extension.
struct IndexedDBPaths {
  base::FilePath blob_path;
  base::FilePath leveldb_path;
};

// Structure containing Ash and Lacros's version of Preferences.
struct PreferencesContents {
  std::string ash;
  std::string lacros;
};

// Chrome instance type (Ash or Lacros).
enum class ChromeType {
  kAsh,
  kLacros,
};

// Preferences's keys that have to be split between Ash and Lacros
// based on extension id.
constexpr const char* kSplitPreferencesKeys[] = {
    "extensions.pinned_extensions", "extensions.settings",
    "extensions.toolbar",           "updateclientdata.apps",
    "web_apps.web_app_ids",
};
// Preferences's keys that should not be migrated to Lacros.
constexpr const char* kAshOnlyPreferencesKeys[] = {
    "app_list.local_state",
    "invalidation.per_sender_active_registration_tokens",
    "invalidation.per_sender_client_id_cache",
    "invalidation.per_sender_registered_for_invalidation",
    "invalidation.per_sender_topics_to_handler",
    "invalidation.topics_to_handler",
};
// Preferences's key that has to be moved to Lacros, and cleared in Ash.
constexpr const char* kLacrosOnlyPreferencesKeys[] = {
    "sync.cache_guid",
};

// List of data types in Sync Data that have to stay in Ash and Ash only.
inline constexpr syncer::DataType kAshOnlySyncDataTypesForLacrosMigration[] = {
    syncer::DataType::APP_LIST,
    syncer::DataType::ARC_PACKAGE,
    syncer::DataType::OS_PREFERENCES,
    syncer::DataType::OS_PRIORITY_PREFERENCES,
    syncer::DataType::PRINTERS,
    syncer::DataType::PRINTERS_AUTHORIZATION_SERVERS,
    syncer::DataType::WIFI_CONFIGURATIONS,
    syncer::DataType::WORKSPACE_DESK,
};

constexpr char kTotalSize[] = "Ash.UserDataStatsRecorder.DataSize.TotalSize";

// UMA name prefix to record sizes of files/dirs in profile data directory. The
// name unique to each file/dir is appended to the end to create a full UMA name
// as follows `Ash.UserDataStatsRecorder.DataSize.{ItemName}`.
constexpr char kUserDataStatsRecorderDataSize[] =
    "Ash.UserDataStatsRecorder.DataSize.";

// Files/dirs that is not assigned a unique uma name is given this name.
constexpr char kUnknownUMAName[] = "Unknown";

// The size of disk space that should be kept free after migration.
// We currently set this to 100MB. Note that this is smaller than the threshold
// of 768MB for aggressive disk cleanup by cryptohome thus migration can cause
// the threshold to be reached triggering aggressive disk cleanup. We allow this
// because 1. migration does not create much extra data (< 50MB for 99.99% of
// users) and 2. migration is not unique in that any other Chrome feature can
// write files to disk resulting in lower disk space.
constexpr uint64_t kBuffer = 100LL * 1024 * 1024;

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
  int64_t total_size = 0;
};

// Specifies the type of `TargetItem`
enum class ItemType {
  kLacros = 0,       // Item that should be moved to lacros profile directory.
  kRemainInAsh = 1,  // Item that should remain in ash.
  kNeedCopyForMove =
      2,  // Item that should be copied to lacros during move migration.
  kNeedCopyForCopy = 3,  // Item that should be copied to lacros during copy
                         // migration. This is kNeedCopyForMove + "Preferences".
  kDeletable = 4,  // Item that can be deleted to free up space i.e. cache.
};

// It enumerates the file/dirs in the given directory and returns items of
// `type`. E.g. `GetTargetItems(path, ItemType::kLacros)` will get all items
// that should be moved to lacros.
TargetItems GetTargetItems(const base::FilePath& original_profile_dir,
                           ItemType type);

// Returns extra bytes that has to be freed for the migration to be carried out
// if there are `total_copy_size` bytes of copying to be done. Returns 0 if no
// extra space needs to be freed.
uint64_t ExtraBytesRequiredToBeFreed(
    int64_t total_copy_size,
    const base::FilePath& original_profile_dir);

// Returns an estimate of the total of file sizes created during profile
// migration in bytes. Note that this underestimates the total because some
// smaller files that are being created during the migration.
int64_t EstimatedExtraBytesCreated(const base::FilePath& original_profile_dir);

// Injects the bytes to be returned by ExtraBytesRequiredToBeFreed above
// in RAII manner.
class ScopedExtraBytesRequiredToBeFreedForTesting {
 public:
  explicit ScopedExtraBytesRequiredToBeFreedForTesting(uint64_t bytes);
  ~ScopedExtraBytesRequiredToBeFreedForTesting();
};

// Copies `items` to `to_dir`.
bool CopyTargetItems(
    const base::FilePath& to_dir,
    const TargetItems& items,
    CancelFlag* cancel_flag,
    standalone_browser::MigrationProgressTracker* progress_tracker);

// Copies `item` to location pointed by `dest`. Returns true on success and
// false on failure.
bool CopyTargetItem(
    const TargetItem& item,
    const base::FilePath& dest,
    CancelFlag* cancel_flag,
    standalone_browser::MigrationProgressTracker* progress_tracker);

// Copies the contents of `from_path` to `to_path` recursively. Unlike
// `base::CopyDirectory()` it skips symlinks.
bool CopyDirectory(
    const base::FilePath& from_path,
    const base::FilePath& to_path,
    CancelFlag* cancel_flag,
    standalone_browser::MigrationProgressTracker* progress_tracker);

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

// Given a key in Sync Data's leveldb, returns true if (based on its prefix) its
// data type has to stay in Ash and Ash only, false otherwise.
bool IsAshOnlySyncDataType(std::string_view key);

// Given an extension id, return the paths of the associated blob
// and leveldb directories inside IndexedDB.
IndexedDBPaths GetIndexedDBPaths(const base::FilePath& profile_path,
                                 const char* extension_id);

// Migrate the LevelDB instance at `original_path` to `target_path`,
// Filter out all the extensions that are not in `kExtensionsAshOnly`.
// `leveldb_type` determines the schema type.
bool MigrateLevelDB(const base::FilePath& original_path,
                    const base::FilePath& target_path,
                    const LevelDBType leveldb_type);

// Migrate Sync Data's LevelDB instance at `original_path` to Ash and Lacros.
// For Ash, filter out the data types that are not meant to be ported to Lacros.
// For Lacros, filter out the data types that are meant to stay in Ash.
bool MigrateSyncDataLevelDB(const base::FilePath& original_path,
                            const base::FilePath& ash_target_path,
                            const base::FilePath& lacros_target_path);

// Manipulates the given representation of Preferences (`root_dict`)
// so that the given key only contains values relevant to Ash or
// Lacros, depending on `chrome_type`.
//
// If the entry in `root_dict` at `key` is a dict in the format
// { <AppId> : { ... }, ... }, it will change the dict to contain only
// AppIds of extensions meant to be in `chrome_type` (Ash or Lacros).
//
// If the entry is a list in the format [ <AppId>, ... ], it will
// change the list to contain only AppIds of extensions meant to be
// in `chrome_type` (Ash or Lacros).
//
// If the entry is a list in any other format, if it doesn't exist,
// or if it's not container type, no changes will be performed.
void UpdatePreferencesKeyByType(base::Value::Dict* root_dict,
                                std::string_view key,
                                ChromeType chrome_type);

// Given a `original_contents` string containing the original Preferences
// file, return the migrated Ash and Lacros versions of Preferences.
std::optional<PreferencesContents> MigratePreferencesContents(
    std::string_view original_contents);

// Migrate Preferences to Ash and Lacros.
bool MigratePreferences(const base::FilePath& original_path,
                        const base::FilePath& ash_target_path,
                        const base::FilePath& lacros_target_path);

// Copy or move IndexedDB objects to Ash's profile directory.
bool MigrateAshIndexedDB(const base::FilePath& src_profile_dir,
                         const base::FilePath& target_indexed_db_dir,
                         const char* extension_id,
                         bool copy);

}  // namespace browser_data_migrator_util

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_CROSAPI_BROWSER_DATA_MIGRATOR_UTIL_H_
