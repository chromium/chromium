// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/extensions/api/image_writer_private/removable_storage_provider.h"
#include "content/public/browser/browser_thread.h"

#if defined(USE_UDEV)
#include "device/udev_linux/scoped_udev.h"
#endif

namespace extensions {
// TODO(haven): Udev code may be duplicated in the Chrome codebase.
// https://code.google.com/p/chromium/issues/detail?id=284898

#if defined(USE_UDEV)
// Returns the integer contained in |attr|.  Returns 0 on error.
static uint64_t get_int_attr(const char* attr) {
  uint64_t result = 0;
  // In error cases, StringToInt will set result to 0
  base::StringToUint64(attr, &result);
  return result;
}

static int get_device_blk_size(const std::string& path) {
  base::FilePath file_path(path);
  std::string device = file_path.BaseName().value();

  base::FilePath info_file_path = base::FilePath("/sys/block")
                                      .Append(device)
                                      .Append("queue/logical_block_size");

  std::string file_contents;
  int blk_size;

  if (!base::ReadFileToString(info_file_path, &file_contents)) {
    return 0;
  }
  // In error cases, StringToInt will set blk_size to 0
  base::StringToInt(file_contents, &blk_size);

  return blk_size;
}
#endif  // defined(USE_UDEV)

// static
scoped_refptr<StorageDeviceList>
RemovableStorageProvider::PopulateDeviceList() {
#if defined(USE_UDEV)
  device::ScopedUdevPtr udev(device::udev_new());
  if (!udev) {
    DLOG(ERROR) << "Can't create udev";
    return nullptr;
  }

  auto device_list = base::MakeRefCounted<StorageDeviceList>();
  /* Create a list of the devices in the 'block' subsystem. */
  device::ScopedUdevEnumeratePtr enumerate(
      device::udev_enumerate_new(udev.get()));

  device::udev_enumerate_add_match_subsystem(enumerate.get(), "block");
  device::udev_enumerate_scan_devices(enumerate.get());
  udev_list_entry* devices =
      device::udev_enumerate_get_list_entry(enumerate.get());

  udev_list_entry* dev_list_entry;
  udev_list_entry_foreach(dev_list_entry, devices) {
    const char* path = device::udev_list_entry_get_name(dev_list_entry);
    device::ScopedUdevDevicePtr cur_device(
        device::udev_device_new_from_syspath(udev.get(), path));

    const char* partition =
        device::udev_device_get_sysattr_value(cur_device.get(), "partition");
    if (partition && get_int_attr(partition)) {
      // This is a partition of a device, not the device itself
      continue;
    }

    const char* removable =
        device::udev_device_get_sysattr_value(cur_device.get(), "removable");
    if (!removable || !get_int_attr(removable)) {
      // This is not a removable storage device.
      continue;
    }

    /* Get the parent SCSI device that contains the model
       and manufacturer.  You can look at the hierarchy with
       udevadm info -a -n /dev/<device> */
    udev_device* parent_device =
        device::udev_device_get_parent_with_subsystem_devtype(cur_device.get(),
                                                              "scsi", nullptr);
    if (!parent_device) {
      // this is not a usb device
      continue;
    }

    api::image_writer_private::RemovableStorageDevice device_item;
    device_item.vendor =
        device::UdevDeviceGetSysattrValue(parent_device, "vendor");
    device_item.model =
        device::UdevDeviceGetSysattrValue(parent_device, "model");
    // TODO (smaskell): Don't expose raw device path
    device_item.storage_unit_id =
        device::udev_device_get_devnode(cur_device.get());
    device_item.capacity = get_int_attr(device::udev_device_get_sysattr_value(
                               cur_device.get(), "size")) *
                           get_device_blk_size(device_item.storage_unit_id);
    device_item.removable = removable;

    device_list->data.push_back(std::move(device_item));
  }

  return device_list;
#else
  return nullptr;
#endif  // defined(USE_UDEV)
}

}  // namespace extensions
