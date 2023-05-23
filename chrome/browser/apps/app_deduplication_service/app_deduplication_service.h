// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_DEDUPLICATION_SERVICE_APP_DEDUPLICATION_SERVICE_H_
#define CHROME_BROWSER_APPS_APP_DEDUPLICATION_SERVICE_APP_DEDUPLICATION_SERVICE_H_

#include <map>
#include <string>

#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/apps/app_deduplication_service/app_deduplication_cache.h"
#include "chrome/browser/apps/app_deduplication_service/app_deduplication_server_connector.h"
#include "chrome/browser/apps/app_deduplication_service/duplicate_group.h"
#include "chrome/browser/apps/app_deduplication_service/entry_types.h"
#include "chrome/browser/apps/app_deduplication_service/proto/deduplication_data.pb.h"
#include "chrome/browser/apps/app_provisioning_service/app_provisioning_data_manager.h"
#include "chrome/browser/apps/app_provisioning_service/proto/app_data.pb.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"

class Profile;

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace apps::deduplication {

class AppDeduplicationService : public KeyedService,
                                public AppProvisioningDataManager::Observer,
                                public apps::AppRegistryCache::Observer {
 public:
  explicit AppDeduplicationService(Profile* profile);
  ~AppDeduplicationService() override;
  AppDeduplicationService(const AppDeduplicationService&) = delete;
  AppDeduplicationService& operator=(const AppDeduplicationService&) = delete;

  // Call this function before using any other function.
  // This function returns true if the Deduplication Service has been
  // properly initialised, ensuring the correctness of method responses.
  bool IsServiceOn();
  std::vector<Entry> GetDuplicates(const EntryId& entry_id);
  bool AreDuplicates(const EntryId& entry_id_1, const EntryId& entry_id_2);

  // Registers prefs used for the App Deduplication Service.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

 private:
  friend class AppDeduplicationServiceTest;
  FRIEND_TEST_ALL_PREFIXES(AppDeduplicationServiceTest,
                           OnDuplicatedGroupListUpdated);
  FRIEND_TEST_ALL_PREFIXES(AppDeduplicationServiceTest,
                           ExactDuplicateAllInstalled);
  FRIEND_TEST_ALL_PREFIXES(AppDeduplicationServiceTest, Installation);
  FRIEND_TEST_ALL_PREFIXES(AppDeduplicationServiceTest, Websites);

  friend class AppDeduplicationServiceAlmanacTest;
  FRIEND_TEST_ALL_PREFIXES(AppDeduplicationServiceAlmanacTest,
                           DeduplicateDataToEntries);
  FRIEND_TEST_ALL_PREFIXES(AppDeduplicationServiceAlmanacTest,
                           DeduplicateDataToEntriesInvalidAppType);
  FRIEND_TEST_ALL_PREFIXES(AppDeduplicationServiceAlmanacTest,
                           DeduplicateDataToEntriesInvalidAppId);
  FRIEND_TEST_ALL_PREFIXES(AppDeduplicationServiceAlmanacTest,
                           PrefUnchangedAfterServerError);
  FRIEND_TEST_ALL_PREFIXES(AppDeduplicationServiceAlmanacTest,
                           PrefSetAfterServerSuccess);
  FRIEND_TEST_ALL_PREFIXES(AppDeduplicationServiceAlmanacTest,
                           ValidServiceNoDuplicates);
  FRIEND_TEST_ALL_PREFIXES(AppDeduplicationServiceAlmanacTest,
                           ValidServiceWithDuplicates);

  enum class EntryStatus {
    // This entry is not an app entry (could be website, phonehub, etc.).
    kNonApp = 0,
    kInstalledApp = 1,
    kNotInstalledApp = 2
  };

  // Starts the process of calling the server to retrieve duplicate app data.
  // A call is only made to the server if there is a difference of over 24 hours
  // between now and the time stored in the server pref.
  void StartLoginFlow();

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
  void GetDeduplicateDataFromServer(DeviceInfo device_info);

  // Processes data retrieved by server connector and stores in disk.
  void OnGetDeduplicateDataFromServerCompleted(
      absl::optional<proto::DeduplicateData> response);

  // Checks for any errors after data is written to cache. If the write is
  // successful, it will call the cache to read from disk.
  void OnWriteDeduplicationCacheCompleted(bool result);

  // Process data read from cache and converts it into `Entry`s.
  void OnReadDeduplicationCacheCompleted(
      absl::optional<proto::DeduplicateData> data);

  // Maps deduplicate data read from disk to `Entry`s which are then stored
  // inside the class as maps.
  void DeduplicateDataToEntries(proto::DeduplicateData data);

  // Gets the pref which stores the last time the client made a call to the
  // server.
  base::Time GetServerPref();

  void GetDeduplicateAppsCompleteCallbackForTesting(
      base::OnceCallback<void(bool)> callback) {
    get_data_complete_callback_for_testing_ = std::move(callback);
  }

  std::map<uint32_t, DuplicateGroup> duplication_map_;
  std::map<EntryId, uint32_t> entry_to_group_map_;
  std::map<EntryId, EntryStatus> entry_status_;
  raw_ptr<Profile, ExperimentalAsh> profile_;

  base::ScopedObservation<AppProvisioningDataManager,
                          AppProvisioningDataManager::Observer>
      app_provisioning_data_observeration_{this};
  base::ScopedObservation<apps::AppRegistryCache,
                          apps::AppRegistryCache::Observer>
      app_registry_cache_observation_{this};

  std::unique_ptr<AppDeduplicationServerConnector> server_connector_;
  std::unique_ptr<DeviceInfoManager> device_info_manager_;
  std::unique_ptr<AppDeduplicationCache> cache_;

  // For testing
  base::OnceCallback<void(bool)> get_data_complete_callback_for_testing_;

  // `weak_ptr_factory_` must be the last member of this class.
  base::WeakPtrFactory<AppDeduplicationService> weak_ptr_factory_{this};
};

}  // namespace apps::deduplication

#endif  // CHROME_BROWSER_APPS_APP_DEDUPLICATION_SERVICE_APP_DEDUPLICATION_SERVICE_H_
