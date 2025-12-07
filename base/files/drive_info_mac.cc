// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/drive_info.h"

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOBSD.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOTypes.h>
#include <IOKit/storage/IOMedia.h>
#include <IOKit/storage/IOStorageDeviceCharacteristics.h>
#include <paths.h>
#include <sys/mount.h>
#include <sys/param.h>

#include <optional>

#include "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/files/file_path.h"
#include "base/mac/scoped_ioobject.h"
#include "base/memory/scoped_policy.h"
#include "base/strings/sys_string_conversions.h"
#include "base/threading/scoped_blocking_call.h"

namespace {

template <typename T>
base::apple::ScopedCFTypeRef<T> QueryParentsForProperty(io_object_t io_object,
                                                        CFStringRef key) {
  base::apple::ScopedCFTypeRef<CFTypeRef> result(
      IORegistryEntrySearchCFProperty(
          io_object, kIOServicePlane, key, kCFAllocatorDefault,
          kIORegistryIterateRecursively | kIORegistryIterateParents));
  if (!base::apple::CFCast<T>(result.get())) {
    return base::apple::ScopedCFTypeRef<T>();
  }

  return base::apple::ScopedCFTypeRef<T>(static_cast<T>(result.release()));
}

}  // namespace

namespace base {

std::optional<DriveInfo> GetIOObjectDriveInfo(io_object_t io_object) {
  if (!IOObjectConformsTo(io_object, kIOMediaClass)) {
    return std::nullopt;
  }
  DriveInfo drive_info;

  // Query parents for the drive medium, which is a device characteristic, and
  // determines whether the drive is rotational (has seek penalty).
  apple::ScopedCFTypeRef<CFDictionaryRef> device_characteristics =
      QueryParentsForProperty<CFDictionaryRef>(
          io_object, CFSTR(kIOPropertyDeviceCharacteristicsKey));
  if (device_characteristics) {
    CFStringRef medium_type = apple::GetValueFromDictionary<CFStringRef>(
        device_characteristics.get(), CFSTR(kIOPropertyMediumTypeKey));
    if (medium_type) {
      if (CFEqual(medium_type, CFSTR(kIOPropertyMediumTypeRotationalKey))) {
        drive_info.has_seek_penalty = true;
      } else if (CFEqual(medium_type,
                         CFSTR(kIOPropertyMediumTypeSolidStateKey))) {
        drive_info.has_seek_penalty = false;
      }
    }
  }

  // Query parents for the physical interconnect (to determine whether a drive
  // is connected over USB), which is a protocol characteristic.
  apple::ScopedCFTypeRef<CFDictionaryRef> protocol_characteristics =
      QueryParentsForProperty<CFDictionaryRef>(
          io_object, CFSTR(kIOPropertyProtocolCharacteristicsKey));
  if (protocol_characteristics) {
    CFStringRef phy_type = apple::GetValueFromDictionary<CFStringRef>(
        protocol_characteristics.get(),
        CFSTR(kIOPropertyPhysicalInterconnectTypeKey));
    if (phy_type) {
      drive_info.is_usb =
          CFEqual(phy_type, CFSTR(kIOPropertyPhysicalInterconnectTypeUSB));
    }
  }

  // Query for the "CoreStorage" property, which is present on CoreStorage
  // volumes.
  apple::ScopedCFTypeRef<CFBooleanRef> cf_corestorage =
      QueryParentsForProperty<CFBooleanRef>(io_object, CFSTR("CoreStorage"));
  // If the property doesn't exist, it's safe to say that this isn't
  // CoreStorage. In any case, starting with Big Sur, CoreStorage functionality
  // has mostly been stripped from the OS.
  drive_info.is_core_storage =
      cf_corestorage && CFBooleanGetValue(cf_corestorage.get());

  drive_info.is_apfs = false;
  {
    mac::ScopedIOObject<io_object_t> current_obj;
    mac::ScopedIOObject<io_object_t> next_obj(io_object, scoped_policy::RETAIN);
    // The UUID for the normal type of APFS physical store. Code that uses
    // GetBSDNameDriveInfo() with a `/dev/diskX` device name may see a physical
    // store.
    CFStringRef kApfsPhysicalStoreUUID =
        CFSTR("7C3457EF-0000-11AA-AA11-00306543ECAC");
    // APFS Container UUID, which resides on a physical store. Manually querying
    // for objects in IOKit with a matching dictionary can obtain these objects.
    CFStringRef kApfsContainerUUID =
        CFSTR("EF57347C-0000-11AA-AA11-00306543ECAC");
    // APFS Volume or Snapshot UUID. A volume resides in a container, while a
    // snapshot is associated with a volume. Code that uses GetFileDriveInfo()
    // will likely obtain a Volume.
    CFStringRef kApfsVolumeOrSnapshotUUID =
        CFSTR("41504653-0000-11AA-AA11-00306543ECAC");
    // Used for iBoot.
    CFStringRef kApfsIBootUUID = CFSTR("69646961-6700-11AA-AA11-00306543ECAC");
    // Used for the recovery system.
    CFStringRef kApfsRecoverySystemUUID =
        CFSTR("69646961-6700-11AA-AA11-00306543ECAC");
    do {
      current_obj = std::move(next_obj);
      apple::ScopedCFTypeRef<CFTypeRef> media_content_cftype(
          IORegistryEntryCreateCFProperty(current_obj.get(),
                                          CFSTR(kIOMediaContentKey),
                                          kCFAllocatorDefault, 0));
      CFStringRef media_content =
          apple::CFCast<CFStringRef>(media_content_cftype.get());

      // A simple case-insensitive comparison for UUIDs is good enough; parsing
      // them for comparison would be overkill.
      auto uuid_equal = [](CFStringRef a, CFStringRef b) {
        return CFStringCompare(a, b, kCFCompareCaseInsensitive) ==
               kCFCompareEqualTo;
      };

      if (media_content) {
        if (uuid_equal(media_content, kApfsPhysicalStoreUUID) ||
            uuid_equal(media_content, kApfsContainerUUID) ||
            uuid_equal(media_content, kApfsVolumeOrSnapshotUUID) ||
            uuid_equal(media_content, kApfsIBootUUID) ||
            uuid_equal(media_content, kApfsRecoverySystemUUID)) {
          drive_info.is_apfs = true;
          break;
        }
      }
    } while ((IORegistryEntryGetParentEntry(current_obj.get(), kIOServicePlane,
                                            next_obj.InitializeInto())) ==
             KERN_SUCCESS);
  }

  // If the media has kIOMediaRemovableKey set to true, mark it as removable.
  // (There is no need to further check kIOMediaEjectableKey as all ejectable
  // media is necessarily removable.)
  //
  // Otherwise, mark external drives as ejectable as well, to match the behavior
  // of the Finder.
  apple::ScopedCFTypeRef<CFBooleanRef> cf_removable =
      QueryParentsForProperty<CFBooleanRef>(io_object,
                                            CFSTR(kIOMediaRemovableKey));
  if (cf_removable && CFBooleanGetValue(cf_removable.get())) {
    drive_info.is_removable = true;
  } else {
    apple::ScopedCFTypeRef<CFStringRef> cf_phy_location =
        QueryParentsForProperty<CFStringRef>(
            io_object, CFSTR(kIOPropertyPhysicalInterconnectLocationKey));
    if (cf_phy_location) {
      drive_info.is_removable =
          CFEqual(cf_phy_location.get(), CFSTR(kIOPropertyExternalKey));
    }
  }

  apple::ScopedCFTypeRef<CFNumberRef> cf_volume_size =
      QueryParentsForProperty<CFNumberRef>(io_object, CFSTR(kIOMediaSizeKey));
  if (cf_volume_size) {
    int64_t size;
    Boolean success =
        CFNumberGetValue(cf_volume_size.get(), kCFNumberSInt64Type, &size);
    if (success) {
      drive_info.size_bytes = size;
    }
  }

  apple::ScopedCFTypeRef<CFBooleanRef> cf_writable =
      QueryParentsForProperty<CFBooleanRef>(io_object,
                                            CFSTR(kIOMediaWritableKey));
  if (cf_writable) {
    drive_info.is_writable = CFBooleanGetValue(cf_writable.get());
  }

  apple::ScopedCFTypeRef<CFStringRef> cf_bsd_name =
      QueryParentsForProperty<CFStringRef>(io_object, CFSTR(kIOBSDNameKey));
  if (cf_bsd_name) {
    drive_info.bsd_name = SysCFStringRefToUTF8(cf_bsd_name.get());
  }

  return drive_info;
}

std::optional<DriveInfo> GetFileDriveInfo(const FilePath& file_path) {
  ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::MAY_BLOCK);

  struct statfs statfs_buf;
  if (statfs(file_path.value().c_str(), &statfs_buf) < 0) {
    return std::nullopt;
  }

  // Remove the "/dev/" from the beginning.
  constexpr char kSlashDevSlash[] = _PATH_DEV;
  std::string dev_path(statfs_buf.f_mntfromname);
  if (dev_path.starts_with(kSlashDevSlash)) {
    dev_path = dev_path.substr(std::size(kSlashDevSlash) - 1);
  }

  apple::ScopedCFTypeRef<CFDictionaryRef> bsd_match_dict(
      IOBSDNameMatching(kIOMainPortDefault, /*options=*/0, dev_path.c_str()));
  if (!bsd_match_dict) {
    return std::nullopt;
  }

  mac::ScopedIOObject<io_object_t> io_media(IOServiceGetMatchingService(
      kIOMainPortDefault, bsd_match_dict.release()));
  if (!io_media) {
    return std::nullopt;
  }

  return GetIOObjectDriveInfo(io_media.get());
}

}  // namespace base
