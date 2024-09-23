// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/volume_mounter/volume_mounter_mojom_traits.h"

#include "base/notreached.h"

namespace mojo {

arc::mojom::DeviceType
EnumTraits<arc::mojom::DeviceType, ash::DeviceType>::ToMojom(
    ash::DeviceType device_type) {
  switch (device_type) {
    case ash::DeviceType::kUSB:
      return arc::mojom::DeviceType::DEVICE_TYPE_USB;
    case ash::DeviceType::kSD:
      return arc::mojom::DeviceType::DEVICE_TYPE_SD;
    case ash::DeviceType::kUnknown:
    case ash::DeviceType::kOpticalDisc:
    case ash::DeviceType::kMobile:
    case ash::DeviceType::kDVD:
      // Android doesn't recognize this device natively. So, propagating
      // UNKNOWN and let Android decides how to handle this.
      return arc::mojom::DeviceType::DEVICE_TYPE_UNKNOWN;
  }
  NOTREACHED();
}

bool EnumTraits<arc::mojom::DeviceType, ash::DeviceType>::FromMojom(
    arc::mojom::DeviceType input,
    ash::DeviceType* out) {
  switch (input) {
    case arc::mojom::DeviceType::DEVICE_TYPE_USB:
      *out = ash::DeviceType::kUSB;
      return true;
    case arc::mojom::DeviceType::DEVICE_TYPE_SD:
      *out = ash::DeviceType::kSD;
      return true;
    case arc::mojom::DeviceType::DEVICE_TYPE_UNKNOWN:
      *out = ash::DeviceType::kUnknown;
      return true;
  }
  NOTREACHED();
}

arc::mojom::MountEvent
EnumTraits<arc::mojom::MountEvent, ash::disks::DiskMountManager::MountEvent>::
    ToMojom(ash::disks::DiskMountManager::MountEvent mount_event) {
  switch (mount_event) {
    case ash::disks::DiskMountManager::MountEvent::MOUNTING:
      return arc::mojom::MountEvent::MOUNTING;
    case ash::disks::DiskMountManager::MountEvent::UNMOUNTING:
      return arc::mojom::MountEvent::UNMOUNTING;
  }
  NOTREACHED();
}

bool EnumTraits<arc::mojom::MountEvent,
                ash::disks::DiskMountManager::MountEvent>::
    FromMojom(arc::mojom::MountEvent input,
              ash::disks::DiskMountManager::MountEvent* out) {
  switch (input) {
    case arc::mojom::MountEvent::MOUNTING:
      *out = ash::disks::DiskMountManager::MountEvent::MOUNTING;
      return true;
    case arc::mojom::MountEvent::UNMOUNTING:
      *out = ash::disks::DiskMountManager::MountEvent::UNMOUNTING;
      return true;
  }
  NOTREACHED();
}

}  // namespace mojo
