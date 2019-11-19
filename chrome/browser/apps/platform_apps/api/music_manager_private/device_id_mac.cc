// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/platform_apps/api/music_manager_private/device_id.h"

#include <CoreFoundation/CoreFoundation.h>
#include <DiskArbitration/DADisk.h>
#include <DiskArbitration/DASession.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/network/IOEthernetController.h>
#include <IOKit/network/IOEthernetInterface.h>
#include <IOKit/network/IONetworkInterface.h>
#include <stddef.h>
#include <sys/mount.h>

#include "base/bind.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/mac/scoped_ioobject.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/post_task.h"
#include "base/threading/scoped_blocking_call.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace chrome_apps {
namespace api {

namespace {

const char kRootDirectory[] = "/";

typedef base::Callback<bool(const void* bytes, size_t size)>
    IsValidMacAddressCallback;

// Return the BSD name (e.g. '/dev/disk1') of the root directory by enumerating
// through the mounted volumes .
// Return "" if an error occured.
std::string FindBSDNameOfSystemDisk() {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  struct statfs* mounted_volumes;
  int num_volumes = getmntinfo(&mounted_volumes, 0);
  if (num_volumes == 0) {
    VLOG(1) << "Cannot enumerate list of mounted volumes.";
    return std::string();
  }

  for (int i = 0; i < num_volumes; i++) {
    struct statfs* vol = &mounted_volumes[i];
    if (std::string(vol->f_mntonname) == kRootDirectory) {
      return std::string(vol->f_mntfromname);
    }
  }

  VLOG(1) << "Cannot find disk mounted as '" << kRootDirectory << "'.";
  return std::string();
}

// Return the Volume UUID property of a BSD disk name (e.g. '/dev/disk1').
// Return "" if an error occured.
std::string GetVolumeUUIDFromBSDName(const std::string& bsd_name) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  const CFAllocatorRef allocator = NULL;

  base::ScopedCFTypeRef<DASessionRef> session(DASessionCreate(allocator));
  if (session.get() == NULL) {
    VLOG(1) << "Error creating DA Session.";
    return std::string();
  }

  base::ScopedCFTypeRef<DADiskRef> disk(
      DADiskCreateFromBSDName(allocator, session, bsd_name.c_str()));
  if (disk.get() == NULL) {
    VLOG(1) << "Error creating DA disk from BSD disk name.";
    return std::string();
  }

  base::ScopedCFTypeRef<CFDictionaryRef> disk_description(
      DADiskCopyDescription(disk));
  if (disk_description.get() == NULL) {
    VLOG(1) << "Error getting disk description.";
    return std::string();
  }

  CFUUIDRef volume_uuid = base::mac::GetValueFromDictionary<CFUUIDRef>(
      disk_description, kDADiskDescriptionVolumeUUIDKey);
  if (volume_uuid == NULL) {
    VLOG(1) << "Error getting volume UUID of disk.";
    return std::string();
  }

  base::ScopedCFTypeRef<CFStringRef> volume_uuid_string(
      CFUUIDCreateString(allocator, volume_uuid));
  if (volume_uuid_string.get() == NULL) {
    VLOG(1) << "Error creating string from CSStringRef.";
    return std::string();
  }

  return base::SysCFStringRefToUTF8(volume_uuid_string.get());
}

// Return Volume UUID property of disk mounted as "/".
std::string GetVolumeUUID() {
  std::string result;
  std::string bsd_name = FindBSDNameOfSystemDisk();
  if (!bsd_name.empty()) {
    VLOG(4) << "BSD name of root directory: '" << bsd_name << "'";
    result = GetVolumeUUIDFromBSDName(bsd_name);
  }
  return result;
}

class MacAddressProcessor {
 public:
  MacAddressProcessor(const IsValidMacAddressCallback& is_valid_mac_address)
      : is_valid_mac_address_(is_valid_mac_address) {}

  bool ProcessNetworkController(io_object_t network_controller) {
    // Use the MAC address of the first network interface.
    bool keep_going = true;
    base::ScopedCFTypeRef<CFDataRef> mac_address_data(
        static_cast<CFDataRef>(IORegistryEntryCreateCFProperty(
            network_controller, CFSTR(kIOMACAddress), kCFAllocatorDefault, 0)));
    if (!mac_address_data)
      return keep_going;

    const UInt8* mac_address = CFDataGetBytePtr(mac_address_data);
    size_t mac_address_size = CFDataGetLength(mac_address_data);
    if (!is_valid_mac_address_.Run(mac_address, mac_address_size))
      return keep_going;

    std::string mac_address_string =
        base::ToLowerASCII(base::HexEncode(mac_address, mac_address_size));

    base::ScopedCFTypeRef<CFStringRef> provider_class(
        static_cast<CFStringRef>(IORegistryEntryCreateCFProperty(
            network_controller, CFSTR(kIOProviderClassKey), kCFAllocatorDefault,
            0)));
    if (provider_class) {
      if (CFStringCompare(provider_class, CFSTR("IOPCIDevice"), 0) ==
          kCFCompareEqualTo) {
        // MAC address from built-in network card is always best choice.
        found_mac_address_ = mac_address_string;
        keep_going = false;
        return keep_going;
      }
    }

    // Fall back to using non built-in card MAC address, but keep looking.
    found_mac_address_ = mac_address_string;
    return keep_going;
  }

  std::string mac_address() const { return found_mac_address_; }

 private:
  const IsValidMacAddressCallback& is_valid_mac_address_;
  std::string found_mac_address_;
};

std::string GetMacAddress(
    const IsValidMacAddressCallback& is_valid_mac_address) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  mach_port_t master_port;
  kern_return_t kr = IOMasterPort(MACH_PORT_NULL, &master_port);
  if (kr != KERN_SUCCESS) {
    LOG(ERROR) << "IOMasterPort failed: " << kr;
    return "";
  }

  CFMutableDictionaryRef match_classes =
      IOServiceMatching(kIOEthernetInterfaceClass);
  if (!match_classes) {
    LOG(ERROR) << "IOServiceMatching returned a NULL dictionary";
    return "";
  }

  io_iterator_t iterator_ref;
  kr = IOServiceGetMatchingServices(master_port, match_classes, &iterator_ref);
  if (kr != KERN_SUCCESS) {
    LOG(ERROR) << "IOServiceGetMatchingServices failed: " << kr;
    return "";
  }
  base::mac::ScopedIOObject<io_iterator_t> iterator(iterator_ref);

  MacAddressProcessor processor(is_valid_mac_address);
  while (true) {
    // Note: interface_service should not be released.
    io_object_t interface_service = IOIteratorNext(iterator);
    if (!interface_service)
      break;

    io_object_t controller_service_ref;
    kr = IORegistryEntryGetParentEntry(interface_service, kIOServicePlane,
                                       &controller_service_ref);
    if (kr != KERN_SUCCESS) {
      LOG(ERROR) << "IORegistryEntryGetParentEntry failed: " << kr;
    } else {
      base::mac::ScopedIOObject<io_object_t> controller_service(
          controller_service_ref);
      bool keep_going = processor.ProcessNetworkController(controller_service);
      if (!keep_going) {
        break;
      }
    }
  }
  return processor.mac_address();
}

void GetRawDeviceIdImpl(const IsValidMacAddressCallback& is_valid_mac_address,
                        const DeviceId::IdCallback& callback) {
  std::string raw_device_id;
  std::string mac_address = GetMacAddress(is_valid_mac_address);
  std::string disk_id = GetVolumeUUID();
  if (!mac_address.empty() && !disk_id.empty()) {
    raw_device_id = mac_address + disk_id;
  }
  base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                 base::BindOnce(callback, raw_device_id));
}

}  // namespace

// static
void DeviceId::GetRawDeviceId(const IdCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::PostTask(
      FROM_HERE, traits(),
      base::Bind(&GetRawDeviceIdImpl, base::Bind(&DeviceId::IsValidMacAddress),
                 callback));
}

}  // namespace api
}  // namespace chrome_apps
