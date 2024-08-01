// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/image_writer_private/removable_storage_provider.h"

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/storage/IOBlockStorageDevice.h>
#include <IOKit/storage/IOMedia.h>
#include <IOKit/storage/IOStorageProtocolCharacteristics.h>
#include <stdint.h>

#include "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/mac/scoped_ioobject.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/sys_string_conversions.h"
#include "base/threading/scoped_blocking_call.h"
#include "chrome/common/extensions/image_writer/image_writer_util_mac.h"

namespace extensions {

// static
scoped_refptr<StorageDeviceList>
RemovableStorageProvider::PopulateDeviceList() {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  // Match only writable whole-disks.
  base::apple::ScopedCFTypeRef<CFMutableDictionaryRef> matching(
      IOServiceMatching(kIOMediaClass));
  CFDictionaryAddValue(matching.get(), CFSTR(kIOMediaWholeKey), kCFBooleanTrue);
  CFDictionaryAddValue(matching.get(), CFSTR(kIOMediaWritableKey),
                       kCFBooleanTrue);

  // IOServiceGetMatchingServices consumes a reference to the matching
  // dictionary passed to it.
  base::mac::ScopedIOObject<io_service_t> disk_iterator;
  if (IOServiceGetMatchingServices(kIOMasterPortDefault, matching.release(),
                                   disk_iterator.InitializeInto()) !=
      KERN_SUCCESS) {
    LOG(ERROR) << "Unable to get disk services.";
    return nullptr;
  }

  base::mac::ScopedIOObject<io_object_t> disk_obj;
  auto device_list = base::MakeRefCounted<StorageDeviceList>();
  while (disk_obj.reset(IOIteratorNext(disk_iterator.get())), disk_obj) {
    std::string bsd_name;
    uint64_t size_in_bytes;
    bool removable;

    bool is_suitable = IsSuitableRemovableStorageDevice(
        disk_obj.get(), &bsd_name, &size_in_bytes, &removable);
    if (!is_suitable) {
      continue;
    }

    base::apple::ScopedCFTypeRef<CFMutableDictionaryRef> dict;
    if (IORegistryEntryCreateCFProperties(disk_obj.get(), dict.InitializeInto(),
                                          kCFAllocatorDefault,
                                          0) != KERN_SUCCESS) {
      LOG(ERROR) << "Unable to get properties of disk object.";
      continue;
    }

    base::apple::ScopedCFTypeRef<CFDictionaryRef> characteristics(
        static_cast<CFDictionaryRef>(IORegistryEntrySearchCFProperty(
            disk_obj.get(), kIOServicePlane,
            CFSTR(kIOPropertyDeviceCharacteristicsKey), kCFAllocatorDefault,
            kIORegistryIterateParents | kIORegistryIterateRecursively)));

    if (!characteristics) {
      LOG(ERROR) << "Unable to find device characteristics for " << bsd_name;
      continue;
    }

    CFStringRef cf_vendor = base::apple::GetValueFromDictionary<CFStringRef>(
        characteristics.get(), CFSTR(kIOPropertyVendorNameKey));
    std::string vendor = base::SysCFStringRefToUTF8(cf_vendor);

    CFStringRef cf_model = base::apple::GetValueFromDictionary<CFStringRef>(
        characteristics.get(), CFSTR(kIOPropertyProductNameKey));
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
