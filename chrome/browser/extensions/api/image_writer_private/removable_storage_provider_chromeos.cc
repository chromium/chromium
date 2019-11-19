// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/image_writer_private/removable_storage_provider.h"

#include "base/memory/scoped_refptr.h"
#include "chromeos/disks/disk.h"
#include "chromeos/disks/disk_mount_manager.h"

namespace extensions {

const char kUnknownSDDiskModel[] = "SD Card";
const char kUnknownUSBDiskModel[] = "USB Drive";

using chromeos::disks::Disk;
using chromeos::disks::DiskMountManager;

// The Chrome OS implementation takes advantage of the Chrome OS
// DiskMountManager.  This does not expose whether the device is a removable or
// fixed disk.  In fact, some SD cards will present themselves as fixed disks
// (see http://crbug.com/340761).  Thus we just expose all USB and SD drives.
// static
scoped_refptr<StorageDeviceList>
RemovableStorageProvider::PopulateDeviceList() {
  DiskMountManager* disk_mount_manager = DiskMountManager::GetInstance();
  const DiskMountManager::DiskMap& disks = disk_mount_manager->disks();
  auto device_list = base::MakeRefCounted<StorageDeviceList>();

  for (DiskMountManager::DiskMap::const_iterator iter = disks.begin();
       iter != disks.end();
       ++iter) {
    const Disk& disk = *iter->second;
    if (disk.is_parent() && !disk.on_boot_device() && disk.has_media() &&
        (disk.device_type() == chromeos::DEVICE_TYPE_USB ||
         disk.device_type() == chromeos::DEVICE_TYPE_SD)) {
      api::image_writer_private::RemovableStorageDevice device;
      device.storage_unit_id = disk.device_path();
      device.capacity = disk.total_size_in_bytes();
      device.removable = disk.on_removable_device();
      device.vendor = disk.vendor_name();
      device.model = disk.product_name();

      if (device.model.empty() && device.vendor.empty()) {
        if (disk.device_type() == chromeos::DEVICE_TYPE_USB) {
          device.model = kUnknownUSBDiskModel;
        } else {
          device.model = kUnknownSDDiskModel;
        }
      }

      device_list->data.push_back(std::move(device));
    }
  }

  return device_list;
}

}  // namespace extensions
