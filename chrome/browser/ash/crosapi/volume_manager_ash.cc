// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/volume_manager_ash.h"

#include <utility>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/profiles/profile.h"

namespace crosapi {

namespace {

bool IsVolumeAvailableToLacros(const file_manager::Volume& volume) {
  // Use type (available only in ash-chrome) to decide whether a volume can be
  // used by lacros-chrome. This is needed because virtual file system support
  // for lacros is WIP.
  // TODO(crbug.com/1296438): Adjust as needed re. VFS support progress, being
  // mindful of version skew and its limits.
  auto type = volume.type();
  return type == file_manager::VOLUME_TYPE_DOWNLOADS_DIRECTORY ||
         type == file_manager::VOLUME_TYPE_REMOVABLE_DISK_PARTITION ||
         type == file_manager::VOLUME_TYPE_MOUNTED_ARCHIVE_FILE ||
         type == file_manager::VOLUME_TYPE_SMB;
}

crosapi::mojom::VolumePtr ConvertVolumeToMojom(
    const file_manager::Volume& src_volume) {
  crosapi::mojom::VolumePtr dst_volume = crosapi::mojom::Volume::New();
  dst_volume->volume_id = src_volume.volume_id();
  dst_volume->volume_label = src_volume.volume_label();
  dst_volume->writable = !src_volume.is_read_only();
  // TODO(crbug.com/1296438): Deprecate once VFS works fully, being mindful of
  // version skew and its limits.
  dst_volume->is_available_to_lacros = IsVolumeAvailableToLacros(src_volume);
  dst_volume->mount_path = src_volume.mount_path();
  return dst_volume;
}

std::vector<crosapi::mojom::VolumePtr> ConvertVolumeListToMojom(
    std::vector<base::WeakPtr<file_manager::Volume>> src_volume_list) {
  std::vector<crosapi::mojom::VolumePtr> dst_volume_list;
  for (const auto& src_volume : src_volume_list) {
    if (src_volume.get())
      dst_volume_list.emplace_back(ConvertVolumeToMojom(*src_volume));
  }
  return dst_volume_list;
}

// Returns volume list converted to crosapi form.
std::vector<crosapi::mojom::VolumePtr> ReadVolumeList(Profile* profile) {
  const std::vector<base::WeakPtr<file_manager::Volume>> volume_list =
      file_manager::VolumeManager::Get(profile)->GetVolumeList();
  return ConvertVolumeListToMojom(volume_list);
}

}  // namespace

VolumeManagerAsh::VolumeManagerAsh() = default;

VolumeManagerAsh::~VolumeManagerAsh() = default;

void VolumeManagerAsh::SetProfile(Profile* profile) {
  CHECK(profile);
  if (profile_ == profile) {
    VLOG(1) << "VolumeManagerAsh service is already initialized. Skip init.";
    return;
  }

  profile_ = profile;
  profile_observation_.Observe(profile_);
}

void VolumeManagerAsh::BindReceiver(
    mojo::PendingReceiver<mojom::VolumeManager> receiver) {
  // profile_ should be set beforehand.
  CHECK(profile_);
  receivers_.Add(this, std::move(receiver));
}

void VolumeManagerAsh::AddVolumeListObserver(
    mojo::PendingRemote<mojom::VolumeListObserver> observer) {
  if (!is_observing_volume_manager_ && profile_) {
    auto* volume_manager = file_manager::VolumeManager::Get(profile_);
    volume_manager->AddObserver(this);
    is_observing_volume_manager_ = true;
  }

  mojo::Remote<mojom::VolumeListObserver> remote(std::move(observer));
  // Note: The observer is NOT provided with the initial value.

  volume_list_observers_.Add(std::move(remote));
}

void VolumeManagerAsh::GetFullVolumeList(GetFullVolumeListCallback callback) {
  if (profile_) {
    std::move(callback).Run(ReadVolumeList(profile_));
  }
}

void VolumeManagerAsh::GetVolumeMountInfo(const std::string& volume_id,
                                          GetVolumeMountInfoCallback callback) {
  if (profile_) {
    base::WeakPtr<file_manager::Volume> src_volume =
        file_manager::VolumeManager::Get(profile_)->FindVolumeById(volume_id);
    std::move(callback).Run(src_volume.get() ? ConvertVolumeToMojom(*src_volume)
                                             : nullptr);
  }
}

void VolumeManagerAsh::OnVolumeMounted(ash::MountError error_code,
                                       const file_manager::Volume& volume) {
  DispatchVolumeList();
}

void VolumeManagerAsh::OnVolumeUnmounted(ash::MountError error_code,
                                         const file_manager::Volume& volume) {
  DispatchVolumeList();
}

void VolumeManagerAsh::OnShutdownStart(
    file_manager::VolumeManager* volume_manager) {
  // Using DCHECK since this function gets called only if observing.
  DCHECK(is_observing_volume_manager_);
  volume_manager->RemoveObserver(this);
  is_observing_volume_manager_ = false;
}

void VolumeManagerAsh::OnProfileWillBeDestroyed(Profile* profile) {
  CHECK_EQ(profile_, profile);
  profile_ = nullptr;
  profile_observation_.Reset();
}

void VolumeManagerAsh::DispatchVolumeList() {
  if (volume_list_observers_.empty() || !profile_) {
    return;
  }
  std::vector<crosapi::mojom::VolumePtr> volume_list = ReadVolumeList(profile_);
  for (auto& observer : volume_list_observers_) {
    std::vector<crosapi::mojom::VolumePtr> volume_list_copy;
    for (auto& volume : volume_list) {
      volume_list_copy.emplace_back(volume->Clone());
    }
    observer->OnVolumeListChanged(std::move(volume_list_copy));
  }
}

}  // namespace crosapi
