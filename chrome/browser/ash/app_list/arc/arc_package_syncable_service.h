// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_ARC_ARC_PACKAGE_SYNCABLE_SERVICE_H_
#define CHROME_BROWSER_ASH_APP_LIST_ARC_ARC_PACKAGE_SYNCABLE_SERVICE_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <unordered_map>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/app_list/arc/arc_app_sync_metrics_helper.h"
#include "chrome/browser/ash/arc/session/arc_session_manager_observer.h"
#include "chrome/browser/sync/glue/sync_start_util.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/sync/model/sync_change.h"
#include "components/sync/model/sync_change_processor.h"
#include "components/sync/model/syncable_service.h"
#include "components/sync/protocol/arc_package_specifics.pb.h"

class Profile;

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

// Class that syncs ARC packages install/uninstall.
class ArcPackageSyncableService : public syncer::SyncableService,
                                  public KeyedService,
                                  public ArcAppListPrefs::Observer,
                                  public ArcSessionManagerObserver {
 public:
  struct SyncItem {
    SyncItem(const std::string& package_name,
             int32_t package_version,
             int64_t last_backup_android_id,
             int64_t last_backup_time);
    const std::string package_name;
    int32_t package_version;
    int64_t last_backup_android_id;
    int64_t last_backup_time;
  };

  // Use ArcPackageSyncableServiceFactory instead.
  ArcPackageSyncableService(Profile* profile, ArcAppListPrefs* prefs);
  ArcPackageSyncableService(const ArcPackageSyncableService&) = delete;
  ArcPackageSyncableService& operator=(const ArcPackageSyncableService&) =
      delete;

  ~ArcPackageSyncableService() override;

  static std::unique_ptr<ArcPackageSyncableService> Create(
      Profile* profile,
      ArcAppListPrefs* prefs);
  static ArcPackageSyncableService* Get(content::BrowserContext* context);

  // Returns true if requested package has pending sync request.
  bool IsPackageSyncing(const std::string& package_name) const;

  // syncer::SyncableService:
  void WaitUntilReadyToSync(base::OnceClosure done) override;
  std::optional<syncer::ModelError> MergeDataAndStartSyncing(
      syncer::DataType type,
      const syncer::SyncDataList& initial_sync_data,
      std::unique_ptr<syncer::SyncChangeProcessor> sync_processor) override;
  void StopSyncing(syncer::DataType type) override;
  std::optional<syncer::ModelError> ProcessSyncChanges(
      const base::Location& from_here,
      const syncer::SyncChangeList& change_list) override;
  base::WeakPtr<SyncableService> AsWeakPtr() override;

  bool SyncStarted();

  // Tries to install/resinstall a package that is in the pending list.
  // Sends the request to Android.
  void InstallPendingPackage(const std::string& package_name,
                             arc::mojom::InstallPriority priority);

 private:
  using SyncItemMap =
      std::unordered_map<std::string, std::unique_ptr<SyncItem>>;

  // ArcAppListPrefs::Observer:
  void OnPackageInstalled(const mojom::ArcPackageInfo& package_info) override;
  void OnPackageModified(const mojom::ArcPackageInfo& package_info) override;
  void OnPackageRemoved(const std::string& package_name,
                        bool uninstalled) override;
  void OnPackageListInitialRefreshed() override;

  // ArcSessionManagerObserver:
  void OnArcSessionStopped(ArcStopReason stop_reason) override;

  // Sends adds/updates sync change to sync server.
  void SendSyncChange(
      const mojom::ArcPackageInfo& package_info,
      const syncer::SyncChange::SyncChangeType& sync_change_type);

  // Creates or updates local syncItem with data change from sync server. Sends
  // request to install/update package to Android.
  bool ProcessSyncItemSpecifics(const sync_pb::ArcPackageSpecifics& specifics);

  // Deletes local syncItem corresponding to data change from sync server.
  // Sends request to uninstall package to Android.
  bool DeleteSyncItemSpecifics(const sync_pb::ArcPackageSpecifics& specifics);

  // Sends uninstall notification for given package to Android.
  void UninstallPackage(const SyncItem* sync_item);

  // Returns if a package should be synced.
  // TODO(lgcheng@) Support may need to be added in this function for different
  // use cases.
  bool ShouldSyncPackage(const std::string& package_name) const;

  // Maybe updates installation info for app sync metrics.
  void MaybeUpdateInstallMetrics(const mojom::ArcPackageInfo& package_info);

  const raw_ptr<Profile> profile_;
  base::OnceClosure wait_until_ready_to_sync_cb_;
  std::unique_ptr<syncer::SyncChangeProcessor> sync_processor_;

  // Items which are synced.
  SyncItemMap sync_items_;

  // Items new from sync service, waiting for confirmation of installation.
  // These items may never be approved for installation and this structure is
  // used to ensure syncer::SyncDataList GetAllSyncData(syncer::DataType type)
  // returns consistent results from different devices.
  // API to re-install pending_install_items_ can be created when needed.
  SyncItemMap pending_install_items_;

  // Items to delete from sync service, waiting for confirmation of
  // uninstallation. These items will no longer be counted in
  // syncer::SyncDataList GetAllSyncData(syncer::DataType type).
  // API to re-uninstall pending_uninstall_items_ can be created when needed.
  SyncItemMap pending_uninstall_items_;

  // Run()ning tells sync to try and start soon, because syncable changes
  // have started happening. It will cause sync to call us back
  // asynchronously via MergeDataAndStartSyncing as soon as possible.
  syncer::SyncableService::StartSyncFlare flare_;

  const raw_ptr<ArcAppListPrefs> prefs_;

  ArcAppSyncMetricsHelper metrics_helper_;

  base::WeakPtrFactory<ArcPackageSyncableService> weak_ptr_factory_{this};
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_APP_LIST_ARC_ARC_PACKAGE_SYNCABLE_SERVICE_H_
