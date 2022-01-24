// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/browser_data_migrator_util.h"

#include <algorithm>

#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"

namespace ash {
namespace browser_data_migrator_util {
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

}  // namespace

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

void RecordUserDataSize(const base::FilePath& path, int64_t size) {
  std::string uma_name = kUserDataStatsRecorderDataSize;
  uma_name += GetUMAItemName(path);

  // Divide 10GB into 100 buckets. Unit in MB.
  LOG(WARNING) << uma_name << ": " << size / 1024 / 1024 << "MB";
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

}  // namespace browser_data_migrator_util
}  // namespace ash
