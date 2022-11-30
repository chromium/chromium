// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_VOLUME_MOUNTER_VOLUME_MOUNTER_MOJOM_TRAITS_H_
#define ASH_COMPONENTS_ARC_VOLUME_MOUNTER_VOLUME_MOUNTER_MOJOM_TRAITS_H_

#include "ash/components/arc/mojom/volume_mounter.mojom-shared.h"
#include "chromeos/ash/components/disks/disk_mount_manager.h"

namespace mojo {

template <>
struct EnumTraits<arc::mojom::DeviceType, ash::DeviceType> {
  static arc::mojom::DeviceType ToMojom(ash::DeviceType device_type);
  static bool FromMojom(arc::mojom::DeviceType input, ash::DeviceType* out);
};

template <>
struct EnumTraits<arc::mojom::MountEvent,
                  ash::disks::DiskMountManager::MountEvent> {
  static arc::mojom::MountEvent ToMojom(
      ash::disks::DiskMountManager::MountEvent mount_event);
  static bool FromMojom(arc::mojom::MountEvent input,
                        ash::disks::DiskMountManager::MountEvent* out);
};

}  // namespace mojo

#endif  // ASH_COMPONENTS_ARC_VOLUME_MOUNTER_VOLUME_MOUNTER_MOJOM_TRAITS_H_
