// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_VOLUME_MANAGER_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_VOLUME_MANAGER_ASH_H_

#include "base/scoped_observation.h"
#include "chrome/browser/ash/file_manager/volume_manager_observer.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "chromeos/ash/components/dbus/cros_disks/cros_disks_client.h"
#include "chromeos/crosapi/mojom/volume_manager.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"

class Profile;

namespace file_manager {
class Volume;
class VolumeManager;
}  // namespace file_manager

namespace crosapi {

// Implements the crosapi volume manager interface. Lives in ash-chrome on the
// UI thread. Allows lacros-chrome to make requests to the Chrome OS volume
// manager.
class VolumeManagerAsh : public mojom::VolumeManager,
                         public file_manager::VolumeManagerObserver,
                         public ProfileObserver {
 public:
  VolumeManagerAsh();
  VolumeManagerAsh(const VolumeManagerAsh&) = delete;
  VolumeManagerAsh& operator=(const VolumeManagerAsh&) = delete;
  ~VolumeManagerAsh() override;

  void SetProfile(Profile* profile);
  void BindReceiver(mojo::PendingReceiver<mojom::VolumeManager> receiver);

  // crosapi::mojom::VolumeManager:
  void AddVolumeListObserver(
      mojo::PendingRemote<mojom::VolumeListObserver> observer) override;
  void GetFullVolumeList(GetFullVolumeListCallback callback) override;
  void GetVolumeMountInfo(const std::string& volume_id,
                          GetVolumeMountInfoCallback callback) override;

  // file_manager::VolumeManagerObserver:
  void OnVolumeMounted(ash::MountError error_code,
                       const file_manager::Volume& volume) override;
  void OnVolumeUnmounted(ash::MountError error_code,
                         const file_manager::Volume& volume) override;
  void OnShutdownStart(file_manager::VolumeManager* volume_manager) override;

  // ProfileObserver:
  void OnProfileWillBeDestroyed(Profile* profile) override;

 private:
  // Reads volume list and and sends copies to |volume_list_observers_|.
  void DispatchVolumeList();

  bool is_observing_volume_manager_ = false;

  mojo::RemoteSet<mojom::VolumeListObserver> volume_list_observers_;

  mojo::ReceiverSet<mojom::VolumeManager> receivers_;

  raw_ptr<Profile> profile_ = nullptr;

  base::ScopedObservation<Profile, ProfileObserver> profile_observation_{this};
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_VOLUME_MANAGER_ASH_H_
