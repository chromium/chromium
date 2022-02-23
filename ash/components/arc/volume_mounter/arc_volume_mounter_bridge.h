// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_VOLUME_MOUNTER_ARC_VOLUME_MOUNTER_BRIDGE_H_
#define ASH_COMPONENTS_ARC_VOLUME_MOUNTER_ARC_VOLUME_MOUNTER_BRIDGE_H_

#include <string>

#include "ash/components/arc/mojom/volume_mounter.mojom.h"
#include "ash/components/arc/session/connection_observer.h"
#include "ash/components/disks/disk_mount_manager.h"
#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/keyed_service/core/keyed_service_base_factory.h"
#include "components/prefs/pref_change_registrar.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

class ArcBridgeService;

// This class handles Volume mount/unmount requests from cros-disks and
// send them to Android.
class ArcVolumeMounterBridge
    : public KeyedService,
      public ash::disks::DiskMountManager::Observer,
      public ConnectionObserver<mojom::VolumeMounterInstance>,
      public mojom::VolumeMounterHost {
 public:
  class Delegate {
   public:
    // To be called by ArcVolumeMounter when a removable media is mounted. This
    // create a watcher for the removable media.
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
  static ArcVolumeMounterBridge* GetForBrowserContextForTesting(
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
      chromeos::MountError error_code,
      const ash::disks::DiskMountManager::MountPointInfo& mount_info) override;

  // mojom::VolumeMounterHost overrides:
  void RequestAllMountPoints() override;
  void ReportMountFailureCount(uint16_t count) override;

  // Initialize ArcVolumeMounterBridge with delegate.
  void Initialize(Delegate* delegate);

  // Send all existing mount events. Usually is called around service startup.
  void SendAllMountEvents();

 private:
  void SendMountEventForMyFiles();
  void SendMountEventForRemovableMedia(
      ash::disks::DiskMountManager::MountEvent event,
      const std::string& source_path,
      const std::string& mount_path,
      const std::string& fs_uuid,
      const std::string& device_label,
      chromeos::DeviceType device_type,
      bool visible);

  bool IsVisibleToAndroidApps(const std::string& uuid) const;
  void OnVisibleStoragesChanged();

  Delegate* delegate_;

  ArcBridgeService* const arc_bridge_service_;  // Owned by ArcServiceManager.

  PrefService* const pref_service_;
  PrefChangeRegistrar change_registerar_;

  base::WeakPtrFactory<ArcVolumeMounterBridge> weak_ptr_factory_{this};
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_VOLUME_MOUNTER_ARC_VOLUME_MOUNTER_BRIDGE_H_
