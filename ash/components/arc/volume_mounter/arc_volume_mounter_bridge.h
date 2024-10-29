// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_VOLUME_MOUNTER_ARC_VOLUME_MOUNTER_BRIDGE_H_
#define ASH_COMPONENTS_ARC_VOLUME_MOUNTER_ARC_VOLUME_MOUNTER_BRIDGE_H_

#include <string>

#include "ash/components/arc/mojom/volume_mounter.mojom.h"
#include "ash/components/arc/session/connection_observer.h"
#include "base/cancelable_callback.h"
#include "base/containers/queue.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/disks/disk_mount_manager.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/keyed_service/core/keyed_service_base_factory.h"
#include "components/prefs/pref_change_registrar.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

constexpr char kArcppMediaSharingServicesJobName[] =
    "arcpp_2dmedia_2dsharing_2dservices";

class ArcBridgeService;

// This class handles Volume mount/unmount requests from cros-disks and
// send them to Android.
class ArcVolumeMounterBridge
    : public KeyedService,
      public ash::disks::DiskMountManager::Observer,
      public ash::disks::DiskMountManager::ArcDelegate,
      public chromeos::PowerManagerClient::Observer,
      public ConnectionObserver<mojom::VolumeMounterInstance>,
      public mojom::VolumeMounterHost {
 public:
  class Delegate {
   public:
    // Returns true if file system changes are watched by file system watchers.
    // Mounting events should be sent to Android only when this returns true so
    // that every file in MyFiles and removable media is indexed in Android's
    // MediaStore.
    virtual bool IsWatchingFileSystemChanges() = 0;

    // To be called by ArcVolumeMounter when a removable media is mounted. This
    // creates a watcher for the removable media if it's not created yet.
    virtual void StartWatchingRemovableMedia(const std::string& fs_uuid,
                                             const std::string& mount_path,
                                             base::OnceClosure callback) = 0;

    // To be called by ArcVolumeMounter when a removable media is unmounted.
    // This removes the watcher for the removable media..
    virtual void StopWatchingRemovableMedia(const std::string& mount_path) = 0;

   protected:
    ~Delegate() = default;
  };
  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcVolumeMounterBridge* GetForBrowserContext(
      content::BrowserContext* context);

  // Returns Factory instance for ArcVolumeMounterBridge.
  static KeyedServiceBaseFactory* GetFactory();

  ArcVolumeMounterBridge(content::BrowserContext* context,
                         ArcBridgeService* bridge_service);

  ArcVolumeMounterBridge(const ArcVolumeMounterBridge&) = delete;
  ArcVolumeMounterBridge& operator=(const ArcVolumeMounterBridge&) = delete;

  ~ArcVolumeMounterBridge() override;

  // ash::disks::DiskMountManager::Observer overrides:
  void OnMountEvent(
      ash::disks::DiskMountManager::MountEvent event,
      ash::MountError error_code,
      const ash::disks::DiskMountManager::MountPoint& mount_info) override;

  // ash::disks::DiskMountManager::ArcDelegate overrides:
  void DropArcCaches(
      const base::FilePath& mount_path,
      ash::disks::DiskMountManager::ArcDelegate::Callback callback) override;

  // chromeos::PowerManagerClient::Observer overrides:
  void SuspendImminent(power_manager::SuspendImminent::Reason reason) override;
  void SuspendDone(base::TimeDelta sleep_duration) override;

  // ConnectionObserver<mojom::VolumeMounterInstance> overrides:
  void OnConnectionReady() override;
  void OnConnectionClosed() override;

  // mojom::VolumeMounterHost overrides:
  void RequestAllMountPoints() override;
  void SetUpExternalStorageMountPoints(
      uint32_t media_provider_uid,
      SetUpExternalStorageMountPointsCallback callback) override;
  void OnReadyToSuspend(bool success) override;

  // Initialize ArcVolumeMounterBridge with delegate.
  void Initialize(Delegate* delegate);

  // Send all existing mount events. Usually is called around service startup.
  void SendAllMountEvents();

  // Utility methods for testing.
  void SetUnmountTimeoutForTesting(const base::TimeDelta& timeout) {
    unmount_timeout_ = timeout;
  }
  base::OneShotTimer* GetUnmountTimerForTesting() { return &unmount_timer_; }

  static void EnsureFactoryBuilt();

 private:
  void SendMountEventForMyFiles();
  void SendMountEventForRemovableMedia(
      ash::disks::DiskMountManager::MountEvent event,
      const std::string& source_path,
      const std::string& mount_path,
      const std::string& fs_uuid,
      const std::string& device_label,
      ash::DeviceType device_type,
      bool visible);

  bool IsVisibleToAndroidApps(const std::string& uuid) const;
  void OnVisibleStoragesChanged();

  bool IsReadyToSendMountingEvents();

  void OnSetUpExternalStorageMountPoints(
      const std::string& job_name,
      SetUpExternalStorageMountPointsCallback callback,
      bool result,
      std::optional<std::string> error_name,
      std::optional<std::string> error_message);

  // Processes the oldest PrepareForRemovableMediaUnmount request queued in
  // `unmount_requests_` by calling the PrepareForRemovableMediaUnmount mojo
  // method and starting `unmount_timer_`.
  void ProcessPendingRemovableMediaUnmountRequest();

  // The callback for PrepareForRemovableMediaUnmount mojo call and
  // `unmount_timer_`. This method should be called only by one of them for
  // every unmount request.
  void OnArcPreparedForRemovableMediaUnmount(const base::FilePath& mount_path,
                                             bool is_timeout,
                                             bool success);

  using UnmountRequest =
      std::tuple<base::FilePath,
                 ash::disks::DiskMountManager::ArcDelegate::Callback>;

  // Pending requests for PrepareForRemovableMediaUnmount().
  base::queue<UnmountRequest> unmount_requests_
      GUARDED_BY_CONTEXT(sequence_checker_);
  // Manages the timeout of PrepareForRemovableMediaUnmount mojo call.
  base::OneShotTimer unmount_timer_ GUARDED_BY_CONTEXT(sequence_checker_);
  // Callback for the current PrepareForRemovableMediaUnmount mojo call.
  // This will be cancelled if not run by the timeout.
  base::CancelableOnceCallback<void(bool)> unmount_mojo_callback_
      GUARDED_BY_CONTEXT(sequence_checker_);
  // Stores the callback passed from DropArcCaches() call that triggered the
  // current in-flight mojo call.
  ash::disks::DiskMountManager::ArcDelegate::Callback unmount_callback_
      GUARDED_BY_CONTEXT(sequence_checker_);
  // When the callback for PrepareForRemovableMediaUnmount mojo does not run
  // within this timeout, the callback will be called with false.
  base::TimeDelta unmount_timeout_ = base::Seconds(30);
  // Holds the last time when PrepareForRemovableMediaUnmount mojo was called.
  base::TimeTicks unmount_mojo_start_time_;

  // Represents the state of cleaning up ARC-side removable media caches before
  // device suspension. State transition should be as follows:
  // NO_SUSPEND -> NOT_READY_TO_SUSPEND:
  //   When `SuspendImminent` is called.
  // NOT_READY_TO_SUSPEND -> READY_TO_SUSPEND:
  //   When `OnReadyToSuspend` is called.
  // NOT_READY_TO_SUSPEND or READY_TO_SUSPEND -> NO_SUSPEND:
  //   When `SuspendDone` is called.
  enum class SuspendState {
    // The device is not going to suspend.
    NO_SUSPEND,
    // The device is going to suspend, but there still might be removable drives
    // mounted on the ARC side.
    NOT_READY_TO_SUSPEND,
    // The device is going to suspend, and all removable drives should have been
    // unmounted on the ARC side.
    READY_TO_SUSPEND,
  };
  SuspendState suspend_state_ = SuspendState::NO_SUSPEND;

  raw_ptr<Delegate, DanglingUntriaged> delegate_ = nullptr;

  const raw_ptr<ArcBridgeService>
      arc_bridge_service_;  // Owned by ArcServiceManager.

  const raw_ptr<PrefService> pref_service_;
  PrefChangeRegistrar change_registerar_;

  bool external_storage_mount_points_are_ready_ = false;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<ArcVolumeMounterBridge> weak_ptr_factory_{this};
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_VOLUME_MOUNTER_ARC_VOLUME_MOUNTER_BRIDGE_H_
