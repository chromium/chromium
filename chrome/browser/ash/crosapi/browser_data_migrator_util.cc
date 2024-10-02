// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/browser_data_migrator_util.h"

#include <unistd.h>

#include <algorithm>
#include <string_view>

#include "base/containers/contains.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_keeplist_chromeos.h"
#include "chrome/common/chrome_constants.h"
#include "chromeos/ash/components/standalone_browser/migration_progress_tracker.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/storage_type.h"
#include "components/sync/model/blocking_data_type_store_impl.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"

namespace ash::browser_data_migrator_util {
namespace {

struct PathNamePair {
  const char* key;
  const char* value;
};

struct PathNameComparator {
  constexpr bool operator()(const PathNamePair& p1,
                            const PathNamePair& p2) const {
    return std::string_view(p1.key) < std::string_view(p2.key);
  }
};

// Key value pairs of path names in profile data directory and their
// corresponding UMA item names.
constexpr PathNamePair kPathNamePairs[] = {
    {"AccountManagerTokens.bin", "AccountManagerTokensBin"},
    {"Accounts", "Accounts"},
    {"Affiliation Database", "AffiliationDatabase"},
    {"AutofillStrikeDatabase", "AutofillStrikeDatabase"},
    {"Bookmarks", "Bookmarks"},
    {"BudgetDatabase", "BudgetDatabase"},
    {"Cache", "Cache"},
    {"Code Cache", "CodeCache"},
    {"Cookies", "Cookies"},
    {"DNR Extension Rules", "DNRExtensionRules"},
    {"Download Service", "DownloadService"},
    {"Downloads", "Downloads"},
    {"Extension Cookies", "ExtensionCookies"},
    {"Extension Rules", "ExtensionRules"},
    {"Extension State", "ExtensionState"},
    {"Extensions", "Extensions"},
    {"Favicons", "Favicons"},
    {"Feature Engagement Tracker", "FeatureEngagementTracker"},
    {"File System", "FileSystem"},
    {"FullRestoreData", "FullRestoreData"},
    {"GCM Store", "GCMStore"},
    {"GCache", "GCache"},
    {"GPUCache", "GPUCache"},
    {"History", "History"},
    {"IndexedDB", "IndexedDB"},
    {"LOCK", "LOCK"},
    {"LOG", "LOG"},
    {"LOG.old", "LOGOld"},
    {"Local App Settings", "LocalAppSettings"},
    {"Local Extension Settings", "LocalExtensionSettings"},
    {"Local Storage", "LocalStorage"},
    {"Login Data", "LoginData"},
    {"Login Data For Account", "LoginDataForAccount"},
    {"Managed Extension Settings", "ManagedExtensionSettings"},
    {"MyFiles", "MyFiles"},
    {"NearbySharePublicCertificateDatabase",
     "NearbySharePublicCertificateDatabase"},
    {"Network Action Predictor", "NetworkActionPredictor"},
    {"Network Persistent State", "NetworkPersistentState"},
    {"PPDCache", "PPDCache"},
    {"Platform Notifications", "PlatformNotifications"},
    {"Policy", "Policy"},
    {"Preferences", "Preferences"},
    {"PreferredApps", "PreferredApps"},
    {"PrintJobDatabase", "PrintJobDatabase"},
    {"QuotaManager", "QuotaManager"},
    {"README", "README"},
    {"RLZ Data", "RLZData"},
    {"Reporting and NEL", "ReportingAndNEL"},
    {"Service Worker", "ServiceWorker"},
    {"Session Storage", "SessionStorage"},
    {"Sessions", "Sessions"},
    {"Shortcuts", "Shortcuts"},
    {"Site Characteristics Database", "SiteCharacteristicsDatabase"},
    {"Storage", "Storage"},
    {"Sync App Settings", "SyncAppSettings"},
    {"Sync Data", "SyncData"},
    {"Sync Extension Settings", "SyncExtensionSettings"},
    {"Top Sites", "TopSites"},
    {"Translate Ranker Model", "TranslateRankerModel"},
    {"TransportSecurity", "TransportSecurity"},
    {"Trusted Vault", "TrustedVault"},
    {"Visited Links", "VisitedLinks"},
    {"Web Applications", "WebApplications"},
    {"Web Data", "WebData"},
    {"WebRTC Logs", "WebRTCLogs"},
    {"app_ranker.pb", "AppRankerPb"},
    {"arc.apps", "ArcApps"},
    {"autobrightness", "Autobrightness"},
    {"blob_storage", "BlobStorage"},
    {"browser_data_migrator", "BrowserDataMigrator"},
    {"crostini.icons", "CrostiniIcons"},
    {"data_reduction_proxy_leveldb", "DataReductionProxyLeveldb"},
    {"databases", "Databases"},
    {"extension_install_log", "ExtensionInstallLog"},
    {"google-assistant-library", "GoogleAssistantLibrary"},
    {"heavy_ad_intervention_opt_out.db", "HeavyAdInterventionOptOutDb"},
    {"lacros", "Lacros"},
    {"login-times", "LoginTimes"},
    {"logout-times", "LogoutTimes"},
    {"optimization_guide_hint_cache_store", "OptimizationGuideHintCacheStore"},
    {"optimization_guide_model_and_features_store",
     "OptimizationGuideModelAndFeaturesStore"},
    {"previews_opt_out.db", "PreviewsOptOutDb"},
    {"shared_proto_db", "SharedProtoDb"},
    {"smartcharging", "Smartcharging"},
    {"structured_metrics", "StructuredMetrics"},
    {"webrtc_event_logs", "WebrtcEventLogs"},
    {"zero_state_group_ranker.pb", "ZeroStateGroupRankerPb"},
    {"zero_state_local_files.pb", "ZeroStateLocalFilesPb"}};

static_assert(base::ranges::is_sorted(kPathNamePairs, PathNameComparator()),
              "kPathNamePairs needs to be sorted by the keys of its elements "
              "so that binary_search can be used on it.");

std::optional<uint64_t> g_extra_bytes_required_to_be_freed_for_testing;

// Key prefixes in LocalStorage's LevelDB.
constexpr char kMetaPrefix[] = "META:chrome-extension://";
constexpr char kKeyPrefix[] = "_chrome-extension://";

// IndexedDB extension suffixes.
constexpr char kIndexedDBBlobExtension[] = ".indexeddb.blob";
constexpr char kIndexedDBLevelDBExtension[] = ".indexeddb.leveldb";

bool ShouldRemoveExtensionByType(std::string_view extension_id,
                                 ChromeType chrome_type) {
  switch (chrome_type) {
    case ChromeType::kAsh:
      return !base::Contains(kExtensionsAshOnly, extension_id) &&
             !base::Contains(
                 extensions::GetExtensionsAndAppsRunInOSAndStandaloneBrowser(),
                 extension_id);

    case ChromeType::kLacros:
      return base::Contains(kExtensionsAshOnly, extension_id);
  }
}

void UpdatePreferencesDictByType(base::Value::Dict& dict,
                                 ChromeType chrome_type) {
  std::vector<std::string> keys_to_remove;

  // Collect keys that don't belong in `chrome_type`.
  for (const auto entry : dict) {
    const std::string_view extension_id = entry.first;
    if (ShouldRemoveExtensionByType(extension_id, chrome_type))
      keys_to_remove.emplace_back(extension_id);
  }

  // Delete those keys.
  for (const std::string& k : keys_to_remove) {
    dict.Remove(k);
  }
}

void UpdatePreferencesListByType(base::Value::List& list,
                                 ChromeType chrome_type) {
  // Erase all elements in the list that don't belong in `chrome_type`.
  list.EraseIf([&](const base::Value& item) {
    if (!item.is_string())
      return false;

    const std::string_view extension_id = item.GetString();
    return ShouldRemoveExtensionByType(extension_id, chrome_type);
  });
}

}  // namespace

CancelFlag::CancelFlag() : cancelled_(false) {}
CancelFlag::~CancelFlag() = default;

TargetItem::TargetItem(base::FilePath path, int64_t size, ItemType item_type)
    : path(std::move(path)),
      size(size),
      is_directory(item_type == ItemType::kDirectory) {}

bool TargetItem::operator==(const TargetItem& rhs) const {
  return this->path == rhs.path && this->size == rhs.size &&
         this->is_directory == rhs.is_directory;
}

TargetItems::TargetItems() = default;
TargetItems::TargetItems(TargetItems&&) = default;
TargetItems::~TargetItems() = default;

// Copies `item` to location pointed by `dest`. Returns true on success and
// false on failure.
bool CopyTargetItem(
    const TargetItem& item,
    const base::FilePath& dest,
    CancelFlag* cancel_flag,
    standalone_browser::MigrationProgressTracker* progress_tracker) {
  if (cancel_flag->IsSet())
    return false;

  if (item.is_directory) {
    if (CopyDirectory(item.path, dest, cancel_flag, progress_tracker))
      return true;
  } else {
    if (base::CopyFile(item.path, dest)) {
      progress_tracker->UpdateProgress(item.size);
      return true;
    }
  }

  PLOG(ERROR) << "Copy failed for " << item.path;
  return false;
}

TargetItems GetTargetItems(const base::FilePath& original_profile_dir,
                           const ItemType type) {
  base::span<const char* const> target_paths;
  switch (type) {
    case ItemType::kLacros:
      target_paths = base::span<const char* const>(kLacrosDataPaths);
      break;
    case ItemType::kRemainInAsh:
      target_paths = base::span<const char* const>(kRemainInAshDataPaths);
      break;
    case ItemType::kDeletable:
      target_paths = base::span<const char* const>(kDeletablePaths);
      break;
    case ItemType::kNeedCopyForMove:
      target_paths = base::span<const char* const>(kNeedCopyForMoveDataPaths);
      break;
    case ItemType::kNeedCopyForCopy:
      target_paths = base::span<const char* const>(kNeedCopyForCopyDataPaths);
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }

  TargetItems target_items;
  base::FileEnumerator enumerator(original_profile_dir, false /* recursive */,
                                  base::FileEnumerator::FILES |
                                      base::FileEnumerator::DIRECTORIES |
                                      base::FileEnumerator::SHOW_SYM_LINKS);
  for (base::FilePath entry = enumerator.Next(); !entry.empty();
       entry = enumerator.Next()) {
    const base::FileEnumerator::FileInfo& info = enumerator.GetInfo();
    int64_t size;
    TargetItem::ItemType item_type;
    if (S_ISREG(info.stat().st_mode)) {
      size = info.GetSize();
      item_type = TargetItem::ItemType::kFile;
    } else if (S_ISDIR(info.stat().st_mode)) {
      size =
          browser_data_migrator_util::ComputeDirectorySizeWithoutLinks(entry);
      item_type = TargetItem::ItemType::kDirectory;
    } else {
      // Skip if `entry` is not a file or directory such as a symlink.
      continue;
    }

    if (base::Contains(target_paths, entry.BaseName().value())) {
      target_items.total_size += size;
      target_items.items.emplace_back(TargetItem{entry, size, item_type});
    }
  }

  return target_items;
}

uint64_t ExtraBytesRequiredToBeFreed(
    const int64_t total_copy_size,
    const base::FilePath& original_profile_dir) {
  if (g_extra_bytes_required_to_be_freed_for_testing)
    return *g_extra_bytes_required_to_be_freed_for_testing;

  const int64_t free_disk_space =
      base::SysInfo::AmountOfFreeDiskSpace(original_profile_dir);
  const int64_t required_disk_space = total_copy_size + kBuffer;

  if (required_disk_space > free_disk_space) {
    LOG(WARNING) << required_disk_space
                 << " bytes of disk space is required but only "
                 << free_disk_space << " bytes are available.";
    return required_disk_space - free_disk_space;
  }

  return 0;
}

int64_t EstimatedExtraBytesCreated(const base::FilePath& original_profile_dir) {
  const TargetItems need_to_copy_items =
      GetTargetItems(original_profile_dir, ItemType::kNeedCopyForMove);

  int64_t total_size = need_to_copy_items.total_size;

  // Get file size of 'Preferences' and 'Sync Data'.
  const base::FilePath preferences_path =
      original_profile_dir.Append(chrome::kPreferencesFilename);
  if (base::PathExists(preferences_path)) {
    int64_t size;
    if (base::GetFileSize(preferences_path, &size)) {
      // For 'Preferences', add size * 2 because we create copies for Lacros and
      // Ash of the same size.
      total_size += size * 2;
    } else {
      PLOG(ERROR) << "Failed to get file size for " << preferences_path.value();
    }
  }

  // For 'Sync Data', the "copies" we create for Ash and Lacros have no
  // overlap thus the total size of the newly created copies is approximately
  // equal to the size of the original database.
  const base::FilePath sync_data_path =
      original_profile_dir.Append(kSyncDataFilePath);
  if (base::PathExists(sync_data_path)) {
    total_size += base::ComputeDirectorySize(sync_data_path);
  }

  return total_size;
}

ScopedExtraBytesRequiredToBeFreedForTesting::
    ScopedExtraBytesRequiredToBeFreedForTesting(uint64_t required_size) {
  DCHECK(!g_extra_bytes_required_to_be_freed_for_testing.has_value());
  g_extra_bytes_required_to_be_freed_for_testing = required_size;
}

ScopedExtraBytesRequiredToBeFreedForTesting::
    ~ScopedExtraBytesRequiredToBeFreedForTesting() {
  g_extra_bytes_required_to_be_freed_for_testing.reset();
}

bool CopyDirectory(
    const base::FilePath& from_path,
    const base::FilePath& to_path,
    CancelFlag* cancel_flag,
    standalone_browser::MigrationProgressTracker* progress_tracker) {
  if (cancel_flag->IsSet())
    return false;

  if (!base::PathExists(to_path) && !base::CreateDirectory(to_path)) {
    PLOG(ERROR) << "CreateDirectory() failed for " << to_path.value();
    return false;
  }

  base::FileEnumerator enumerator(from_path, false /* recursive */,
                                  base::FileEnumerator::FILES |
                                      base::FileEnumerator::DIRECTORIES |
                                      base::FileEnumerator::SHOW_SYM_LINKS);
  for (base::FilePath entry = enumerator.Next(); !entry.empty();
       entry = enumerator.Next()) {
    if (cancel_flag->IsSet())
      return false;

    const base::FileEnumerator::FileInfo& info = enumerator.GetInfo();

    // Only copy a file or a dir i.e. skip other types like symlink since
    // copying those might introdue a security risk.
    if (S_ISREG(info.stat().st_mode)) {
      if (!base::CopyFile(entry, to_path.Append(entry.BaseName())))
        return false;

      progress_tracker->UpdateProgress(info.GetSize());
    } else if (S_ISDIR(info.stat().st_mode)) {
      if (!CopyDirectory(entry, to_path.Append(entry.BaseName()), cancel_flag,
                         progress_tracker)) {
        return false;
      }
    }
  }

  return true;
}

bool CopyTargetItems(
    const base::FilePath& to_dir,
    const TargetItems& target_items,
    CancelFlag* cancel_flag,
    standalone_browser::MigrationProgressTracker* progress_tracker) {
  for (const auto& item : target_items.items) {
    if (cancel_flag->IsSet())
      return false;

    if (!CopyTargetItem(item, to_dir.Append(item.path.BaseName()), cancel_flag,
                        progress_tracker)) {
      return false;
    }
  }

  return true;
}

int64_t ComputeDirectorySizeWithoutLinks(const base::FilePath& dir_path) {
  base::FileEnumerator enumerator(dir_path, false /* recursive */,
                                  base::FileEnumerator::FILES |
                                      base::FileEnumerator::DIRECTORIES |
                                      base::FileEnumerator::SHOW_SYM_LINKS);
  int64_t size = 0;
  for (base::FilePath entry = enumerator.Next(); !entry.empty();
       entry = enumerator.Next()) {
    const base::FileEnumerator::FileInfo& info = enumerator.GetInfo();

    if (S_ISREG(info.stat().st_mode)) {
      size += info.GetSize();
    } else if (S_ISDIR(info.stat().st_mode)) {
      size += ComputeDirectorySizeWithoutLinks(entry);
    } else {
      // Skip links.
      continue;
    }
  }

  return size;
}

void RecordTotalSize(int64_t size) {
  base::UmaHistogramCustomCounts(kTotalSize, size / 1024 / 1024, 1, 10000, 100);
}

void RecordTargetItemSizes(const std::vector<TargetItem>& items) {
  for (auto& item : items)
    browser_data_migrator_util::RecordUserDataSize(item.path, item.size);
}

void RecordUserDataSize(const base::FilePath& path, int64_t size) {
  std::string uma_name = kUserDataStatsRecorderDataSize;
  uma_name += GetUMAItemName(path);

  // Divide 10GB into 100 buckets. Unit in MB.
  base::UmaHistogramCustomCounts(uma_name, size / 1024 / 1024, 1, 10000, 100);
}

std::string GetUMAItemName(const base::FilePath& path) {
  std::string path_name = path.BaseName().value();

  auto* it = std::lower_bound(
      std::begin(kPathNamePairs), std::end(kPathNamePairs),
      PathNamePair{path_name.c_str(), nullptr}, PathNameComparator());

  if (it != std::end(kPathNamePairs) &&
      std::string_view(it->key) == path_name) {
    return it->value;
  }

  // If `path_name` was not found in kPathNamePairs, return "Unknown" as name.
  return kUnknownUMAName;
}

void DryRunToCollectUMA(const base::FilePath& profile_data_dir) {
  TargetItems lacros_items =
      GetTargetItems(profile_data_dir, ItemType::kLacros);
  TargetItems need_copy_items =
      GetTargetItems(profile_data_dir, ItemType::kNeedCopyForMove);
  TargetItems remain_in_ash_items =
      GetTargetItems(profile_data_dir, ItemType::kRemainInAsh);
  TargetItems deletable_items =
      GetTargetItems(profile_data_dir, ItemType::kDeletable);

  base::UmaHistogramCustomCounts(kDryRunNoCopyDataSize,
                                 deletable_items.total_size / 1024 / 1024, 1,
                                 10000, 100);
  base::UmaHistogramCustomCounts(kDryRunAshDataSize,
                                 remain_in_ash_items.total_size / 1024 / 1024,
                                 1, 10000, 100);
  base::UmaHistogramCustomCounts(kDryRunLacrosDataSize,
                                 lacros_items.total_size / 1024 / 1024, 1,
                                 10000, 100);
  base::UmaHistogramCustomCounts(kDryRunCommonDataSize,
                                 need_copy_items.total_size / 1024 / 1024, 1,
                                 10000, 100);

  const int64_t total_items_size =
      need_copy_items.total_size + lacros_items.total_size +
      remain_in_ash_items.total_size + deletable_items.total_size;
  browser_data_migrator_util::RecordTotalSize(total_items_size);

  RecordTargetItemSizes(deletable_items.items);
  RecordTargetItemSizes(remain_in_ash_items.items);
  RecordTargetItemSizes(lacros_items.items);
  RecordTargetItemSizes(need_copy_items.items);

  const int64_t extra_bytes_created_by_move =
      EstimatedExtraBytesCreated(profile_data_dir);
  const int64_t free_disk_space =
      base::SysInfo::AmountOfFreeDiskSpace(profile_data_dir);
  const int64_t free_disk_space_after_delete =
      free_disk_space + deletable_items.total_size;
  const int64_t free_disk_space_after_migration =
      free_disk_space_after_delete - extra_bytes_created_by_move;

  base::UmaHistogramCustomCounts(kDryRunExtraDiskSpaceOccupiedByMove,
                                 extra_bytes_created_by_move / 1024 / 1024, 1,
                                 10000, 100);
  base::UmaHistogramCustomCounts(kDryRunFreeDiskSpaceAfterDelete,
                                 free_disk_space_after_delete / 1024 / 1024, 1,
                                 10000, 100);
  base::UmaHistogramCustomCounts(kDryRunFreeDiskSpaceAfterMigration,
                                 free_disk_space_after_migration / 1024 / 1024,
                                 -10000, 10000, 200);

  if (free_disk_space_after_migration < (int64_t)kBuffer) {
    base::UmaHistogramCustomCounts(
        kDryRunExtraDiskSpaceOccupiedByMoveLowDiskUser2,
        extra_bytes_created_by_move / 1024 / 1024, 1, 10000, 100);
    base::UmaHistogramCustomCounts(kDryRunFreeDiskSpaceLowDiskUser2,
                                   free_disk_space / 1024 / 1024, 1, 10000,
                                   100);
    base::UmaHistogramCustomCounts(kDryRunFreeDiskSpaceAfterDeleteLowDiskUser2,
                                   free_disk_space_after_delete / 1024 / 1024,
                                   1, 10000, 100);
    base::UmaHistogramCustomCounts(
        kDryRunProfileDirSizeLowDiskUser2,
        ComputeDirectorySizeWithoutLinks(profile_data_dir) / 1024 / 1024, 1,
        10000, 100);
    base::UmaHistogramCustomCounts(
        kDryRunMyFilesDirSizeLowDiskUser2,
        ComputeDirectorySizeWithoutLinks(profile_data_dir.Append("MyFiles")) /
            1024 / 1024,
        1, 10000, 100);
  }
}

leveldb::Status GetExtensionKeys(leveldb::DB* db,
                                 LevelDBType leveldb_type,
                                 ExtensionKeys* result) {
  std::unique_ptr<leveldb::Iterator> it(
      db->NewIterator(leveldb::ReadOptions()));

  // Iterate through all the elements of the leveldb database.
  for (it->SeekToFirst(); it->Valid(); it->Next()) {
    std::string extension_id;
    const std::string key = it->key().ToString();

    switch (leveldb_type) {
      // LocalStorage format.
      // Refer to: components/services/storage/dom_storage/local_storage_impl.cc
      case LevelDBType::kLocalStorage:
        if (base::StartsWith(key, kMetaPrefix)) {
          extension_id = key.substr(std::size(kMetaPrefix) - 1);
        } else if (base::StartsWith(key, kKeyPrefix)) {
          size_t pos = std::size(kKeyPrefix) - 1;
          size_t end = key.find('\x00', pos);
          if (end != std::string::npos)
            extension_id = key.substr(pos, end - pos);
        }
        break;

      // StateStore format (e.g. `Extension State/Rules`).
      // Refer to: extensions/browser/state_store.cc
      case LevelDBType::kStateStore:
        size_t separator = key.find('.');
        if (separator != std::string::npos)
          extension_id = key.substr(0, separator);
        break;
    }

    // Collect keys associated with each extension id.
    if (!extension_id.empty())
      (*result)[extension_id].push_back(key);
  }

  PLOG_IF(ERROR, !it->status().ok())
      << "GetExtensionKeys() failed with status: " << it->status().ToString();

  return it->status();
}

bool IsAshOnlySyncDataType(std::string_view key) {
  for (auto type : kAshOnlySyncDataTypesForLacrosMigration) {
    if ((base::StartsWith(
             key, FormatDataPrefix(type, syncer::StorageType::kUnspecified)) ||
         base::StartsWith(
             key, FormatMetaPrefix(type, syncer::StorageType::kUnspecified)) ||
         key == FormatGlobalMetadataKey(type,
                                        syncer::StorageType::kUnspecified))) {
      return true;
    }
  }
  return false;
}

IndexedDBPaths GetIndexedDBPaths(const base::FilePath& profile_path,
                                 const char* extension_id) {
  const base::FilePath indexed_db_dir = profile_path.Append(kIndexedDBFilePath);
  const base::FilePath base_path = indexed_db_dir.Append(
      "chrome_extension_" + std::string(extension_id) + "_0");

  return {
      base_path.AddExtension(kIndexedDBBlobExtension),
      base_path.AddExtension(kIndexedDBLevelDBExtension),
  };
}

bool MigrateLevelDB(const base::FilePath& original_path,
                    const base::FilePath& target_path,
                    const LevelDBType leveldb_type) {
  // LevelDB options.
  leveldb_env::Options options;
  options.create_if_missing = false;

  // Open the original LevelDB database.
  std::unique_ptr<leveldb::DB> original_db;
  leveldb::Status status =
      leveldb_env::OpenDB(options, original_path.value(), &original_db);
  if (!status.ok()) {
    PLOG(ERROR) << "Failure while opening original leveldb: " << original_path
                << ": " << status.ToString();
    return false;
  }

  // Retrieve all extensions' keys, indexed by extension id.
  ExtensionKeys original_keys;
  status = GetExtensionKeys(original_db.get(), leveldb_type, &original_keys);
  if (!status.ok()) {
    PLOG(ERROR) << "Failure while reading keys from original leveldb: "
                << original_path << ": " << status.ToString();
    return false;
  }

  // Create a new LevelDB database to store entries that will stay in Ash.
  std::unique_ptr<leveldb::DB> target_db;
  options.create_if_missing = true;
  options.error_if_exists = true;
  status = leveldb_env::OpenDB(options, target_path.value(), &target_db);
  if (!status.ok()) {
    PLOG(ERROR) << "Failure while opening new leveldb: " << target_path << ": "
                << status.ToString();
    return false;
  }

  // Prepare new LevelDB database according to schema.
  // Refer to:
  // - components/services/storage/dom_storage/local_storage_impl.cc
  // - extensions/browser/state_store.cc
  leveldb::WriteBatch write_batch;
  if (leveldb_type == LevelDBType::kLocalStorage) {
    write_batch.Put("VERSION", "1");
  }

  // Copy all the key-value pairs that need to be kept in Ash.
  for (const auto& [extension_id, keys] : original_keys) {
    if (base::Contains(kExtensionsAshOnly, extension_id) ||
        base::Contains(
            extensions::GetExtensionsAndAppsRunInOSAndStandaloneBrowser(),
            extension_id)) {
      for (const std::string& key : keys) {
        std::string value;
        status = original_db->Get(leveldb::ReadOptions(), key, &value);
        if (!status.ok()) {
          PLOG(ERROR) << "Failure while reading from original leveldb: "
                      << original_path << ": " << status.ToString();
          return false;
        }
        write_batch.Put(key, value);
      }
    }
  }

  // Write everything in bulk.
  leveldb::WriteOptions write_options;
  write_options.sync = true;
  status = target_db->Write(write_options, &write_batch);
  if (!status.ok()) {
    PLOG(ERROR) << "Failure while writing into new leveldb: " << target_path
                << ": " << status.ToString();
    return false;
  }

  return true;
}

bool MigrateSyncDataLevelDB(const base::FilePath& original_path,
                            const base::FilePath& ash_target_path,
                            const base::FilePath& lacros_target_path) {
  // Open the original LevelDB database.
  std::unique_ptr<leveldb::DB> original_db;
  leveldb_env::Options options;
  options.create_if_missing = false;
  leveldb::Status status =
      leveldb_env::OpenDB(options, original_path.value(), &original_db);
  if (!status.ok()) {
    PLOG(ERROR) << "Failure while opening original leveldb: " << original_path
                << ": " << status.ToString();
    return false;
  }

  // Create a new LevelDB database to store entries that will stay in Ash.
  std::unique_ptr<leveldb::DB> ash_target_db;
  options.create_if_missing = true;
  options.error_if_exists = true;
  status =
      leveldb_env::OpenDB(options, ash_target_path.value(), &ash_target_db);
  if (!status.ok()) {
    PLOG(ERROR) << "Failure while opening new leveldb: " << ash_target_path
                << ": " << status.ToString();
    return false;
  }

  // Create a new LevelDB database to store entries that will migrate to Lacros.
  std::unique_ptr<leveldb::DB> lacros_target_db;
  status = leveldb_env::OpenDB(options, lacros_target_path.value(),
                               &lacros_target_db);
  if (!status.ok()) {
    PLOG(ERROR) << "Failure while opening new leveldb: " << lacros_target_path
                << ": " << status.ToString();
    return false;
  }

  // Split the key-value pairs between Ash and Lacros.
  leveldb::WriteBatch ash_write_batch;
  leveldb::WriteBatch lacros_write_batch;
  // Iterate through all the elements of the leveldb database.
  std::unique_ptr<leveldb::Iterator> it(
      original_db->NewIterator(leveldb::ReadOptions()));
  for (it->SeekToFirst(); it->Valid(); it->Next()) {
    const std::string key = it->key().ToString();
    const std::string value = it->value().ToString();
    if (IsAshOnlySyncDataType(key))
      ash_write_batch.Put(key, value);
    else
      lacros_write_batch.Put(key, value);
  }
  if (!it->status().ok()) {
    PLOG(ERROR) << "Failure while reading from original leveldb: "
                << original_path << ": " << status.ToString();
    return false;
  }

  // Write everything in bulk.
  leveldb::WriteOptions write_options;
  write_options.sync = true;
  status = ash_target_db->Write(write_options, &ash_write_batch);
  if (!status.ok()) {
    PLOG(ERROR) << "Failure while writing into new leveldb: " << ash_target_path
                << ": " << status.ToString();
    return false;
  }
  status = lacros_target_db->Write(write_options, &lacros_write_batch);
  if (!status.ok()) {
    PLOG(ERROR) << "Failure while writing into new leveldb: "
                << lacros_target_path << ": " << status.ToString();
    return false;
  }

  return true;
}

void UpdatePreferencesKeyByType(base::Value::Dict* root_dict,
                                std::string_view key,
                                ChromeType chrome_type) {
  base::Value* value = root_dict->FindByDottedPath(key);
  if (!value)
    return;

  if (value->is_dict()) {
    UpdatePreferencesDictByType(value->GetDict(), chrome_type);
  } else if (value->is_list()) {
    UpdatePreferencesListByType(value->GetList(), chrome_type);
  }
}

std::optional<PreferencesContents> MigratePreferencesContents(
    std::string_view original_contents) {
  // Parse the original JSON file from Ash.
  std::optional<base::Value> ash_root =
      base::JSONReader::Read(original_contents);
  if (!ash_root) {
    PLOG(ERROR) << "Failure while parsing Ash's Preferences";
    return std::nullopt;
  }
  base::Value::Dict* ash_root_dict = ash_root->GetIfDict();
  if (!ash_root_dict) {
    PLOG(ERROR) << "Failure while parsing Ash's Preferences root node";
    return std::nullopt;
  }

  // Create a copy for Lacros migration.
  base::Value lacros_root = ash_root->Clone();
  base::Value::Dict* lacros_root_dict = lacros_root.GetIfDict();
  if (!lacros_root_dict) {
    PLOG(ERROR) << "Failure while parsing Lacros's Preferences root node";
    return std::nullopt;
  }

  // Some preferences are to be moved to Lacros, and deleted in Ash.
  for (const char* key : kLacrosOnlyPreferencesKeys) {
    base::Value* result = ash_root_dict->FindByDottedPath(key);
    if (result)
      ash_root_dict->RemoveByDottedPath(key);
  }

  // Some preferences don't need to be copied to Lacros.
  for (const char* key : kAshOnlyPreferencesKeys) {
    base::Value* result = lacros_root_dict->FindByDottedPath(key);
    if (result)
      lacros_root_dict->RemoveByDottedPath(key);
  }

  // Some preferences need to be split between Ash and Lacros.
  for (const char* key : kSplitPreferencesKeys) {
    UpdatePreferencesKeyByType(ash_root_dict, key, ChromeType::kAsh);
    UpdatePreferencesKeyByType(lacros_root_dict, key, ChromeType::kLacros);
  }

  // Sync feature setup should not be triggered after migration and should be
  // assumed completed. In Lacros it is controlled by the preference below, but
  // this preference doesn't exist in Ash, so need to set it explicitly here.
  lacros_root_dict->SetByDottedPath(
      kSyncInitialSyncFeatureSetupCompletePrefName, base::Value(true));

  // Generate the resulting JSON.
  PreferencesContents contents;
  if (!base::JSONWriter::Write(*ash_root, &contents.ash)) {
    PLOG(ERROR) << "Failure while generating Ash's Preferences";
    return std::nullopt;
  }
  if (!base::JSONWriter::Write(lacros_root, &contents.lacros)) {
    PLOG(ERROR) << "Failure while generating Lacros's Preferences";
    return std::nullopt;
  }

  return contents;
}

bool MigratePreferences(const base::FilePath& original_path,
                        const base::FilePath& ash_target_path,
                        const base::FilePath& lacros_target_path) {
  std::string original_contents;
  if (!base::ReadFileToString(original_path, &original_contents)) {
    PLOG(ERROR) << "Failure while opening original Preferences: "
                << original_path.value();
    return false;
  }

  auto contents = MigratePreferencesContents(original_contents);
  if (!contents)
    return false;

  if (!base::WriteFile(ash_target_path, contents->ash)) {
    PLOG(ERROR) << "Failure while writing Ash's Preferences: "
                << ash_target_path.value();
    return false;
  }
  if (!base::WriteFile(lacros_target_path, contents->lacros)) {
    PLOG(ERROR) << "Failure while writing Lacros's Preferences: "
                << lacros_target_path.value();
    return false;
  }

  return true;
}

bool MigrateAshIndexedDB(const base::FilePath& src_profile_dir,
                         const base::FilePath& target_indexed_db_dir,
                         const char* extension_id,
                         bool copy) {
  auto MigratePath = [&](const base::FilePath& from, const base::FilePath& to) {
    return copy ? base::CopyDirectory(from, to, /*recursive=*/true)
                : base::Move(from, to);
  };

  const auto& [blob_path, leveldb_path] =
      browser_data_migrator_util::GetIndexedDBPaths(src_profile_dir,
                                                    extension_id);
  if (base::PathExists(blob_path)) {
    const base::FilePath ash_blob_path =
        target_indexed_db_dir.Append(blob_path.BaseName());
    if (!MigratePath(blob_path, ash_blob_path)) {
      PLOG(ERROR) << "Failed migrating " << blob_path.value() << " to "
                  << ash_blob_path.value();
      return false;
    }
  }
  if (base::PathExists(leveldb_path)) {
    const base::FilePath ash_leveldb_path =
        target_indexed_db_dir.Append(leveldb_path.BaseName());
    if (!MigratePath(leveldb_path, target_indexed_db_dir)) {
      PLOG(ERROR) << "Failed migrating " << leveldb_path.value() << " to "
                  << ash_leveldb_path.value();
      return false;
    }
  }

  return true;
}

}  // namespace ash::browser_data_migrator_util
