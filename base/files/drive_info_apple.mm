// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/drive_info.h"

#if BUILDFLAG(IS_MAC)
#include <CoreFoundation/CoreFoundation.h>
#include <DiskArbitration/DiskArbitration.h>
#import <Foundation/Foundation.h>
#include <IOKit/IOBSD.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/storage/IOMedia.h>
#include <IOKit/storage/IOStorageDeviceCharacteristics.h>
#include <sys/stat.h>
#endif

#include <IOKit/IOTypes.h>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_MAC)
#include "base/apple/bridging.h"
#include "base/apple/foundation_util.h"
#include "base/apple/mach_logging.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/files/file_path.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_ioobject.h"
#include "base/strings/sys_string_conversions.h"
#endif

#if BUILDFLAG(IS_MAC)
namespace {
template <typename T>
T QueryParentsForProperty(io_object_t io_object, const char* key) {
  return static_cast<T>(IORegistryEntrySearchCFProperty(
      io_object, kIOServicePlane,
      CFStringCreateWithCString(kCFAllocatorDefault, key,
                                kCFStringEncodingUTF8),
      kCFAllocatorDefault,
      kIORegistryIterateRecursively | kIORegistryIterateParents));
}
}  // namespace
#endif

namespace base {

DriveInfo::DriveInfo() = default;
DriveInfo::~DriveInfo() = default;
DriveInfo::DriveInfo(DriveInfo&&) = default;
DriveInfo& DriveInfo::operator=(DriveInfo&&) = default;

std::optional<DriveInfo> GetIOObjectDriveInfo(io_object_t io_object) {
#if BUILDFLAG(IS_IOS)
  DriveInfo info;
  info.has_seek_penalty = false;
  return info;
#else  // BUILDFLAG(IS_MAC)
  if (!IOObjectConformsTo(io_object, kIOMediaClass)) {
    return std::nullopt;
  }
  DriveInfo drive_info;

  // Query parents for the drive medium, which is a device characteristic, and
  // determines whether the drive is rotational (has seek penalty).
  apple::ScopedCFTypeRef<CFDictionaryRef> device_characteristics(
      QueryParentsForProperty<CFDictionaryRef>(
          io_object, kIOPropertyDeviceCharacteristicsKey));
  if (device_characteristics) {
    CFStringRef type_ref = apple::GetValueFromDictionary<CFStringRef>(
        device_characteristics.get(), CFSTR(kIOPropertyMediumTypeKey));
    if (type_ref) {
      NSString* type = apple::CFToNSPtrCast(type_ref);
      if ([type isEqualToString:@kIOPropertyMediumTypeRotationalKey]) {
        drive_info.has_seek_penalty = true;
      } else if ([type isEqualToString:@kIOPropertyMediumTypeSolidStateKey]) {
        drive_info.has_seek_penalty = false;
      }
    }
  }

  // Query parents for the physical interconnect (to determine whether a drive
  // is connected over USB), which is a protocol characteristic.
  apple::ScopedCFTypeRef<CFDictionaryRef> protocol_characteristics(
      QueryParentsForProperty<CFDictionaryRef>(
          io_object, kIOPropertyProtocolCharacteristicsKey));
  if (protocol_characteristics) {
    CFStringRef phy_type_ref = apple::GetValueFromDictionary<CFStringRef>(
        protocol_characteristics.get(),
        CFSTR(kIOPropertyPhysicalInterconnectTypeKey));
    if (phy_type_ref) {
      drive_info.is_usb = [apple::CFToNSPtrCast(phy_type_ref)
          isEqualToString:@kIOPropertyPhysicalInterconnectTypeUSB];
    }
  }

  // Query for the "CoreStorage" property, which is present on CoreStorage
  // volumes.
  apple::ScopedCFTypeRef<CFBooleanRef> cf_corestorage(
      QueryParentsForProperty<CFBooleanRef>(io_object, "CoreStorage"));
  // If the property doesn't exist, it's safe to say that this isn't
  // CoreStorage. In any case, starting with Big Sur, CoreStorage
  // functionality has mostly been stripped from the OS.
  drive_info.is_core_storage =
      cf_corestorage && CFBooleanGetValue(cf_corestorage.get());

  drive_info.is_apfs = false;
  // Don't delete the IOObject of interest as we release each object in the
  // heirarchy.
  IOObjectRetain(io_object);
  {
    io_object_t current_obj = 0;
    io_object_t next_obj = io_object;
    // The GUID for the normal type of APFS physical store. Code that uses
    // GetBSDNameDriveInfo() with a `/dev/diskX` device name may see a physical
    // store.
    constexpr NSString* kApfsPhysicalStoreGUID =
        @"7C3457EF-0000-11AA-AA11-00306543ECAC";
    // APFS Container GUID, which resides on a physical store. Manually querying
    // for objects in IOKit with a matching dictionary can obtain these objects.
    constexpr NSString* kApfsContainerGUID =
        @"EF57347C-0000-11AA-AA11-00306543ECAC";
    // APFS Volume or Snapshot GUID. A volume resides in a container, while a
    // snapshot is associated with a volume. Code that uses GetFileDriveInfo()
    // will likely obtain a Volume.
    constexpr NSString* kApfsVolumeOrSnapshotGUID =
        @"41504653-0000-11AA-AA11-00306543ECAC";
    // Used for iBoot.
    constexpr NSString* kApfsIBootGUID =
        @"69646961-6700-11AA-AA11-00306543ECAC";
    // Used for the recovery system.
    constexpr NSString* kApfsRecoverySystemGUID =
        @"69646961-6700-11AA-AA11-00306543ECAC";
    // Keep scoped object outside the loop so the object lives to the next
    // GetParentEntry.
    mac::ScopedIOObject<io_object_t> current_obj_ref(current_obj);
    do {
      current_obj = next_obj;
      current_obj_ref.reset(current_obj);
      apple::ScopedCFTypeRef<CFStringRef> cf_content(
          static_cast<CFStringRef>(IORegistryEntryCreateCFProperty(
              current_obj, CFSTR(kIOMediaContentKey), kCFAllocatorDefault, 0)));
      if (cf_content) {
        auto* ns_content = apple::CFToNSPtrCast(cf_content.get());
        if ([ns_content isEqualToString:kApfsPhysicalStoreGUID] ||
            [ns_content isEqualToString:kApfsContainerGUID] ||
            [ns_content isEqualToString:kApfsVolumeOrSnapshotGUID] ||
            [ns_content isEqualToString:kApfsIBootGUID] ||
            [ns_content isEqualToString:kApfsRecoverySystemGUID]) {
          drive_info.is_apfs = true;
          break;
        }
      }
    } while ((IORegistryEntryGetParentEntry(current_obj, kIOServicePlane,
                                            &next_obj)) == KERN_SUCCESS);
  }

  // There is a key for determining whether media is removable, though this does
  // not tell the whole story. Disk Utility uses a combination of determining
  // whether a disk is ejectable and whether it is stored internally to the
  // device to determine whether it is removable, this behavior is emulated
  // here. It also only respects the value of `kIOMediaRemovableKey` and
  // `kIOMediaEjectableKey` if they are true.
  //
  // Interesting to note is that removable hard drives (hard drive enclosures)
  // are not marked as removable according to IOKit despite definitely being
  // removable in reality (can eject in finder, etc), and this is due to them
  // being marked as External).
  apple::ScopedCFTypeRef<CFBooleanRef> cf_removable(
      QueryParentsForProperty<CFBooleanRef>(io_object, kIOMediaRemovableKey));
  if (cf_removable && CFBooleanGetValue(cf_removable.get())) {
    drive_info.is_removable = true;
  } else {
    apple::ScopedCFTypeRef<CFBooleanRef> cf_ejectable(
        QueryParentsForProperty<CFBooleanRef>(io_object, kIOMediaEjectableKey));
    if (cf_ejectable && CFBooleanGetValue(cf_ejectable.get())) {
      drive_info.is_removable = true;
    } else {
      apple::ScopedCFTypeRef<CFStringRef> cf_phy_location(
          QueryParentsForProperty<CFStringRef>(
              io_object, kIOPropertyPhysicalInterconnectLocationKey));
      if (cf_phy_location) {
        drive_info.is_removable = [apple::CFToNSPtrCast(cf_phy_location.get())
            isEqualToString:@kIOPropertyExternalKey];
      }
    }
  }

  apple::ScopedCFTypeRef<CFNumberRef> cf_volume_size(
      QueryParentsForProperty<CFNumberRef>(io_object, kIOMediaSizeKey));
  if (cf_volume_size) {
    int64_t size;
    Boolean success =
        CFNumberGetValue(cf_volume_size.get(), kCFNumberSInt64Type, &size);
    if (success) {
      drive_info.size_bytes = size;
    }
  }

  apple::ScopedCFTypeRef<CFBooleanRef> cf_writable(
      QueryParentsForProperty<CFBooleanRef>(io_object, kIOMediaWritableKey));
  if (cf_writable) {
    drive_info.is_writable = CFBooleanGetValue(cf_writable.get());
  }

  apple::ScopedCFTypeRef<CFStringRef> cf_bsd_name(
      QueryParentsForProperty<CFStringRef>(io_object, kIOBSDNameKey));
  if (cf_bsd_name) {
    drive_info.bsd_name = SysCFStringRefToUTF8(cf_bsd_name.get());
  }

  return drive_info;
#endif
}

std::optional<DriveInfo> GetBSDNameDriveInfo(const std::string_view bsd_name) {
#if BUILDFLAG(IS_IOS)
  DriveInfo info;
  info.has_seek_penalty = false;
  return info;
#else  // BUILDFLAG(IS_MAC)
  std::string full_name("/dev/");
  full_name.append(bsd_name);
  apple::ScopedCFTypeRef<DASessionRef> session(
      DASessionCreate(kCFAllocatorDefault));
  if (!session) {
    return std::nullopt;
  }
  apple::ScopedCFTypeRef<DADiskRef> disk(DADiskCreateFromBSDName(
      kCFAllocatorDefault, session.get(), full_name.c_str()));
  if (!disk) {
    return std::nullopt;
  }
  mac::ScopedIOObject<io_object_t> io_media(DADiskCopyIOMedia(disk.get()));
  return GetIOObjectDriveInfo(io_media.get());
#endif
}

std::optional<DriveInfo> GetFileDriveInfo(const FilePath& file_path) {
#if BUILDFLAG(IS_IOS)
  DriveInfo info;
  info.has_seek_penalty = false;
  return info;
#else  // BUILDFLAG(IS_MAC)
  DriveInfo drive_info;
  struct stat path_stat;
  if (stat(file_path.value().c_str(), &path_stat) < 0) {
    return std::nullopt;
  }
  char devname_buf[256];
  const char* dev_name = devname_r(path_stat.st_dev, S_IFBLK, devname_buf, 256);
  if (!dev_name) {
    return std::nullopt;
  }
  return GetBSDNameDriveInfo(dev_name);
#endif
}

}  // namespace base
