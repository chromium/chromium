// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/image_writer_private/removable_storage_provider.h"

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOBSD.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/storage/IOBlockStorageDevice.h>
#include <IOKit/storage/IOMedia.h>
#include <IOKit/storage/IOStorageProtocolCharacteristics.h>
#include <stdint.h>

#include "base/mac/foundation_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/mac/scoped_ioobject.h"
#include "base/strings/sys_string_conversions.h"
#include "base/threading/scoped_blocking_call.h"
#include "chrome/common/extensions/image_writer/image_writer_util_mac.h"

namespace extensions {

// static
scoped_refptr<StorageDeviceList>
RemovableStorageProvider::PopulateDeviceList() {
  base::ScopedBlockingCall scoped_blocking_call(base::BlockingType::MAY_BLOCK);
  // Match only writable whole-disks.
  CFMutableDictionaryRef matching = IOServiceMatching(kIOMediaClass);
  CFDictionaryAddValue(matching, CFSTR(kIOMediaWholeKey), kCFBooleanTrue);
  CFDictionaryAddValue(matching, CFSTR(kIOMediaWritableKey), kCFBooleanTrue);

  io_service_t disk_iterator;
  if (IOServiceGetMatchingServices(
          kIOMasterPortDefault, matching, &disk_iterator) != KERN_SUCCESS) {
    LOG(ERROR) << "Unable to get disk services.";
    return nullptr;
  }
  base::mac::ScopedIOObject<io_service_t> iterator_ref(disk_iterator);

  io_object_t disk_obj;
  scoped_refptr<StorageDeviceList> device_list(new StorageDeviceList());
  while ((disk_obj = IOIteratorNext(disk_iterator))) {
    base::mac::ScopedIOObject<io_object_t> disk_obj_ref(disk_obj);

    CFMutableDictionaryRef dict;
    if (IORegistryEntryCreateCFProperties(
            disk_obj, &dict, kCFAllocatorDefault, 0) != KERN_SUCCESS) {
      LOG(ERROR) << "Unable to get properties of disk object.";
      continue;
    }
    base::ScopedCFTypeRef<CFMutableDictionaryRef> dict_ref(dict);

    CFStringRef cf_bsd_name = base::mac::GetValueFromDictionary<CFStringRef>(
        dict, CFSTR(kIOBSDNameKey));
    std::string bsd_name = base::SysCFStringRefToUTF8(cf_bsd_name);

    CFNumberRef size_number = base::mac::GetValueFromDictionary<CFNumberRef>(
        dict, CFSTR(kIOMediaSizeKey));
    uint64_t size_in_bytes = 0;
    if (size_number)
      CFNumberGetValue(size_number, kCFNumberLongLongType, &size_in_bytes);

    CFBooleanRef cf_removable = base::mac::GetValueFromDictionary<CFBooleanRef>(
        dict, CFSTR(kIOMediaRemovableKey));
    bool removable = CFBooleanGetValue(cf_removable);

    bool is_usb = IsUsbDevice(disk_obj);

    if (!removable && !is_usb) {
      continue;
    }

    base::ScopedCFTypeRef<CFDictionaryRef> characteristics(
        static_cast<CFDictionaryRef>(IORegistryEntrySearchCFProperty(
            disk_obj,
            kIOServicePlane,
            CFSTR(kIOPropertyDeviceCharacteristicsKey),
            kCFAllocatorDefault,
            kIORegistryIterateParents | kIORegistryIterateRecursively)));

    if (characteristics == NULL) {
      LOG(ERROR) << "Unable to find device characteristics for " << cf_bsd_name;
      continue;
    }

    CFStringRef cf_vendor = base::mac::GetValueFromDictionary<CFStringRef>(
        characteristics, CFSTR(kIOPropertyVendorNameKey));
    std::string vendor = base::SysCFStringRefToUTF8(cf_vendor);

    CFStringRef cf_model = base::mac::GetValueFromDictionary<CFStringRef>(
        characteristics, CFSTR(kIOPropertyProductNameKey));
    std::string model = base::SysCFStringRefToUTF8(cf_model);

    api::image_writer_private::RemovableStorageDevice device;
    device.storage_unit_id = bsd_name;
    device.capacity = size_in_bytes;
    device.vendor = vendor;
    device.model = model;
    device.removable = removable;
    device_list->data.push_back(std::move(device));
  }

  return device_list;
}

} // namespace extensions
