// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/browser_data_migrator_util.h"

#include <unistd.h>

#include <algorithm>

#include "base/containers/contains.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
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
    return base::StringPiece(p1.key) < base::StringPiece(p2.key);
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

absl::optional<uint64_t> g_extra_bytes_required_to_be_freed_for_testing;

// Key prefixes in LocalStorage's LevelDB.
constexpr char kMetaPrefix[] = "META:chrome-extension://";
constexpr char kKeyPrefix[] = "_chrome-extension://";

}  // namespace

CancelFlag::CancelFlag() : cancelled_(false) {}
CancelFlag::~CancelFlag() = default;

TargetItem::TargetItem(base::FilePath path, int64_t size, ItemType item_type)
    : path(path), size(size), is_directory(item_type == ItemType::kDirectory) {}

bool TargetItem::operator==(const TargetItem& rhs) const {
  return this->path == rhs.path && this->size == rhs.size &&
         this->is_directory == rhs.is_directory;
}

TargetItems::TargetItems() : total_size(0) {}
TargetItems::TargetItems(TargetItems&&) = default;
TargetItems::~TargetItems() = default;

// Copies `item` to location pointed by `dest`. Returns true on success and
// false on failure.
bool CopyTargetItem(const TargetItem& item,
                    const base::FilePath& dest,
                    CancelFlag* cancel_flag,
                    MigrationProgressTracker* progress_tracker) {
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
    case ItemType::kNeedCopy:
      target_paths = base::span<const char* const>(kNeedCopyDataPaths);
      break;
    default:
      NOTREACHED();
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

bool HasEnoughDiskSpace(const int64_t total_copy_size,
                        const base::FilePath& original_profile_dir) {
  uint64_t extra_bytes_required_to_be_freed =
      ExtraBytesRequiredToBeFreed(total_copy_size, original_profile_dir);

  return extra_bytes_required_to_be_freed == 0;
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

ScopedExtraBytesRequiredToBeFreedForTesting::
    ScopedExtraBytesRequiredToBeFreedForTesting(uint64_t required_size) {
  DCHECK(!g_extra_bytes_required_to_be_freed_for_testing.has_value());
  g_extra_bytes_required_to_be_freed_for_testing = required_size;
}

ScopedExtraBytesRequiredToBeFreedForTesting::
    ~ScopedExtraBytesRequiredToBeFreedForTesting() {
  g_extra_bytes_required_to_be_freed_for_testing.reset();
}

bool CopyDirectory(const base::FilePath& from_path,
                   const base::FilePath& to_path,
                   CancelFlag* cancel_flag,
                   MigrationProgressTracker* progress_tracker) {
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

bool CreateHardLink(const base::FilePath& from_file,
                    const base::FilePath& to_file) {
  if (link(from_file.value().c_str(), to_file.value().c_str()) == -1) {
    // Note that `link(from_file, to_file)` fails if `to_file` already exists.
    PLOG(ERROR) << "link(" << from_file.value() << ", " << to_file.value()
                << ") failed.";
    return false;
  }

  return true;
}

bool CopyDirectoryByHardLinks(const base::FilePath& from_dir,
                              const base::FilePath& to_dir) {
  if (!base::DirectoryExists(from_dir)) {
    LOG(ERROR) << "from_dir = " << from_dir.value() << " does not exist.";
    return false;
  }

  if (base::PathExists(to_dir)) {
    LOG(ERROR) << "to_dir = " << to_dir.value() << " already exists.";
    return false;
  }

  if (!base::CreateDirectory(to_dir)) {
    PLOG(ERROR) << "Failed base::CreateDirectory(" << to_dir.value() << ").";
    return false;
  }

  base::FileEnumerator enumerator(from_dir, false /* recursive */,
                                  base::FileEnumerator::FILES |
                                      base::FileEnumerator::DIRECTORIES |
                                      base::FileEnumerator::SHOW_SYM_LINKS);
  for (base::FilePath entry = enumerator.Next(); !entry.empty();
       entry = enumerator.Next()) {
    const base::FileEnumerator::FileInfo& info = enumerator.GetInfo();

    // Only create hard links for files/dirs and skip other types like symlink
    // since creating hard links for those might introdue a security risk.
    if (S_ISREG(info.stat().st_mode)) {
      if (!CreateHardLink(entry, to_dir.Append(entry.BaseName())))
        return false;
    } else if (S_ISDIR(info.stat().st_mode)) {
      if (!CopyDirectoryByHardLinks(entry, to_dir.Append(entry.BaseName())))
        return false;
    }
  }

  return true;
}

bool CopyTargetItemsByHardLinks(const base::FilePath& to_dir,
                                const TargetItems& target_items,
                                CancelFlag* cancel_flag) {
  for (const auto& item : target_items.items) {
    if (cancel_flag->IsSet())
      return false;

    if (item.is_directory) {
      if (!CopyDirectoryByHardLinks(item.path,
                                    to_dir.Append(item.path.BaseName()))) {
        return false;
      }
    } else {
      if (!CreateHardLink(item.path, to_dir.Append(item.path.BaseName())))
        return false;
    }
  }

  return true;
}

bool CopyTargetItems(const base::FilePath& to_dir,
                     const TargetItems& target_items,
                     CancelFlag* cancel_flag,
                     MigrationProgressTracker* progress_tracker) {
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

  if (it != std::end(kPathNamePairs) && base::StringPiece(it->key) == path_name)
    return it->value;

  // If `path_name` was not found in kPathNamePairs, return "Unknown" as name.
  return kUnknownUMAName;
}

void DryRunToCollectUMA(const base::FilePath& profile_data_dir) {
  TargetItems lacros_items =
      GetTargetItems(profile_data_dir, ItemType::kLacros);
  TargetItems need_copy_items =
      GetTargetItems(profile_data_dir, ItemType::kNeedCopy);
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

  const int64_t total_copy_size_for_copy_migration =
      need_copy_items.total_size + lacros_items.total_size;
  const int64_t total_copy_size_for_move_migration = need_copy_items.total_size;

  base::UmaHistogramCustomCounts(
      kDryRunCopyMigrationTotalCopySize,
      total_copy_size_for_copy_migration / 1024 / 1024, 1, 10000, 100);
  base::UmaHistogramCustomCounts(
      kDryRunMoveMigrationTotalCopySize,
      total_copy_size_for_move_migration / 1024 / 1024, 1, 10000, 100);

  RecordTargetItemSizes(deletable_items.items);
  RecordTargetItemSizes(remain_in_ash_items.items);
  RecordTargetItemSizes(lacros_items.items);
  RecordTargetItemSizes(need_copy_items.items);

  base::UmaHistogramBoolean(
      kDryRunCopyMigrationHasEnoughDiskSpace,
      HasEnoughDiskSpace(lacros_items.total_size + need_copy_items.total_size,
                         profile_data_dir));
  base::UmaHistogramBoolean(
      kDryRunMoveMigrationHasEnoughDiskSpace,
      HasEnoughDiskSpace(need_copy_items.total_size, profile_data_dir));
  base::UmaHistogramBoolean(
      kDryRunDeleteAndCopyMigrationHasEnoughDiskSpace,
      HasEnoughDiskSpace(lacros_items.total_size + need_copy_items.total_size -
                             deletable_items.total_size,
                         profile_data_dir));
  base::UmaHistogramBoolean(kDryRunDeleteAndMoveMigrationHasEnoughDiskSpace,
                            HasEnoughDiskSpace(need_copy_items.total_size -
                                                   deletable_items.total_size,
                                               profile_data_dir));

  const int64_t free_disk_space =
      base::SysInfo::AmountOfFreeDiskSpace(profile_data_dir);
  const int64_t extra_space_reserved_for_move_migration =
      free_disk_space - need_copy_items.total_size +
      deletable_items.total_size - kBuffer;
  if (extra_space_reserved_for_move_migration > 0) {
    base::UmaHistogramCustomCounts(
        kDryRunMoveMigrationExtraSpaceReserved,
        extra_space_reserved_for_move_migration / 1024 / 1024, 1, 10000, 100);
  } else {
    base::UmaHistogramCustomCounts(
        kDryRunMoveMigrationExtraSpaceRequired,
        -extra_space_reserved_for_move_migration / 1024 / 1024, 1, 10000, 100);
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

  return it->status();
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
    PLOG(ERROR) << "Failure while opening original leveldb: " << original_path;
    return false;
  }

  // Retrieve all extensions' keys, indexed by extension id.
  ExtensionKeys original_keys;
  status = GetExtensionKeys(original_db.get(), leveldb_type, &original_keys);
  if (!status.ok()) {
    PLOG(ERROR) << "Failure while reading keys from original leveldb: "
                << original_path;
    return false;
  }

  // Create a new LevelDB database to store entries that will stay in Ash.
  std::unique_ptr<leveldb::DB> target_db;
  options.create_if_missing = true;
  options.error_if_exists = true;
  status = leveldb_env::OpenDB(options, target_path.value(), &target_db);
  if (!status.ok()) {
    PLOG(ERROR) << "Failure while opening new leveldb: " << target_path;
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
    if (base::Contains(kExtensionKeepList, extension_id)) {
      for (const std::string& key : keys) {
        std::string value;
        status = original_db->Get(leveldb::ReadOptions(), key, &value);
        if (!status.ok()) {
          PLOG(ERROR) << "Failure while reading from original leveldb: "
                      << original_path;
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
    PLOG(ERROR) << "Failure while writing into new leveldb: " << target_path;
    return false;
  }

  return true;
}

}  // namespace ash::browser_data_migrator_util
