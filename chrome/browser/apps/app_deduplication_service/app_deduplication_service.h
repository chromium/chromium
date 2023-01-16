// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_DEDUPLICATION_SERVICE_APP_DEDUPLICATION_SERVICE_H_
#define CHROME_BROWSER_APPS_APP_DEDUPLICATION_SERVICE_APP_DEDUPLICATION_SERVICE_H_

#include <map>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/apps/app_deduplication_service/app_deduplication_server_connector.h"
#include "chrome/browser/apps/app_deduplication_service/duplicate_group.h"
#include "chrome/browser/apps/app_deduplication_service/entry_types.h"
#include "chrome/browser/apps/app_deduplication_service/proto/deduplication_data.pb.h"
#include "chrome/browser/apps/app_provisioning_service/app_provisioning_data_manager.h"
#include "chrome/browser/apps/app_provisioning_service/proto/app_data.pb.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"

class Profile;

namespace apps::deduplication {

class AppDeduplicationService : public KeyedService,
                                public AppProvisioningDataManager::Observer,
                                public apps::AppRegistryCache::Observer {
 public:
  explicit AppDeduplicationService(Profile* profile);
  ~AppDeduplicationService() override;
  AppDeduplicationService(const AppDeduplicationService&) = delete;
  AppDeduplicationService& operator=(const AppDeduplicationService&) = delete;

  std::vector<Entry> GetDuplicates(const EntryId& entry_id);
  bool AreDuplicates(const EntryId& entry_id_1, const EntryId& entry_id_2);

 private:
  friend class AppDeduplicationServiceTest;
  FRIEND_TEST_ALL_PREFIXES(AppDeduplicationServiceTest,
                           OnDuplicatedGroupListUpdated);
  FRIEND_TEST_ALL_PREFIXES(AppDeduplicationServiceTest,
                           ExactDuplicateAllInstalled);
  FRIEND_TEST_ALL_PREFIXES(AppDeduplicationServiceTest, Installation);
  FRIEND_TEST_ALL_PREFIXES(AppDeduplicationServiceTest, Websites);

  enum class EntryStatus {
    // This entry is not an app entry (could be website, phonehub, etc.).
    kNonApp = 0,
    kInstalledApp = 1,
    kNotInstalledApp = 2
  };

  // AppProvisioningDataManager::Observer:
  void OnDuplicatedGroupListUpdated(
      const proto::DuplicatedGroupList& duplicated_apps_map) override;

  // apps::AppRegistryCache::Observer:
  void OnAppUpdate(const apps::AppUpdate& update) override;
  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override;

  void UpdateInstallationStatus(const apps::AppUpdate& update);

  // Search if this entry id belongs to any of the duplicate group.
  // Returns the map key of the duplicate group in the duplication map if a
  // group is found, and return nullptr if the entry id doesn't belong to
  // and duplicate group.
  absl::optional<uint32_t> FindDuplicationIndex(const EntryId& entry_id);

  // Calls server connector to make a request to the Fondue server to retrieve
  // duplicate app group data.
  void GetDeduplicateDataFromServer();

  // Processes data retrieved by server connector and stores in disk.
  void OnGetDeduplicateDataFromServerCompleted(
      absl::optional<proto::DeduplicateData> response);

  std::map<uint32_t, DuplicateGroup> duplication_map_;
  std::map<EntryId, uint32_t> entry_to_group_map_;
  std::map<EntryId, EntryStatus> entry_status_;
  Profile* profile_;

  base::ScopedObservation<AppProvisioningDataManager,
                          AppProvisioningDataManager::Observer>
      app_provisioning_data_observeration_{this};
  base::ScopedObservation<apps::AppRegistryCache,
                          apps::AppRegistryCache::Observer>
      app_registry_cache_observation_{this};

  std::unique_ptr<AppDeduplicationServerConnector> server_connector_;

  // `weak_ptr_factory_` must be the last member of this class.
  base::WeakPtrFactory<AppDeduplicationService> weak_ptr_factory_{this};
};

}  // namespace apps::deduplication

#endif  // CHROME_BROWSER_APPS_APP_DEDUPLICATION_SERVICE_APP_DEDUPLICATION_SERVICE_H_
