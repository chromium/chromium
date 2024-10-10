// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_FILES_DRIVE_INFO_H_
#define BASE_FILES_DRIVE_INFO_H_

#include <optional>

#include "base/base_export.h"
#include "base/files/file_path.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_MAC)
#include <IOKit/IOKitLib.h>
#endif

namespace base {
// Used to hold information about either a drive, or of a combination of a
// partition residing on a drive and the drive itself, depending on how the
// object was constructed. In general, when calling GetFileDriveInfo(), this
// latter case is the one which should be considered. On MacOS, whole media can
// be queried by using by calling GetIOObjectDriveInfo() with an `IOObject`
// obtained via IOServiceGetMatchingService() with `kIOMediaWholeKey` set to
// `true`.
//
// If you add more fields to this structure (platform-specific fields are OK),
// make sure to update all functions that use it in
// file_util_{win|posix|apple}.{cc,mm}, too.
class BASE_EXPORT DriveInfo {
 public:
  DriveInfo();
  DriveInfo(const DriveInfo&) = delete;
  DriveInfo(DriveInfo&&);
  DriveInfo& operator=(const DriveInfo&) = delete;
  DriveInfo& operator=(DriveInfo&&);
  ~DriveInfo();

  // Whether the drive has a seek penalty (i.e. is or is not a spinning disk).
  std::optional<bool> has_seek_penalty;

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
  // Whether the drive is a removable drive.
  //
  // Note on macOS: that SSDs that are connected over USB that you can eject
  // in finder are not necessarily removable and/or ejectable according to
  // IOKitLib. The reason for this is unknown. The same SSD on Windows is
  // non-ejectable (in explorer), and marked as non-removable here.
  std::optional<bool> is_removable;

  // The size of the media, in bytes.
  std::optional<int64_t> size_bytes;
#endif
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  // Whether the drive is connected over USB.
  std::optional<bool> is_usb;
#endif
#if BUILDFLAG(IS_MAC)
  // Whether the drive is a CoreStorage volume.
  std::optional<bool> is_core_storage;

  // Whether the drive is an APFS container.
  std::optional<bool> is_apfs;

  // Whether the drive can be written to.
  std::optional<bool> is_writable;

  // The BSD name is the filename at which the drive is found under /dev. For
  // example, the 3rd partition of the 3rd disk is "disk3s3".
  std::optional<std::string> bsd_name;
#endif
};

// Returns information about the drive on which sits the given file. Also see
// |DriveInfo| - in particular, both the drive itself as well as
// individual attributes thereof can fail to be collected, depending on the
// drive.
BASE_EXPORT std::optional<DriveInfo> GetFileDriveInfo(
    const FilePath& file_path);

#if BUILDFLAG(IS_MAC)
// BSD name is the file found under `/dev`, not the full path including "/dev".
BASE_EXPORT std::optional<DriveInfo> GetBSDNameDriveInfo(
    const std::string_view bsd_name);

// The IO Object is the underlying handle to the drive device. This function can
// be used if already iterating over drives matching certain characteristics.
// This function fails when the `io_object` does not conform to
// `kIOMediaClass`.
BASE_EXPORT std::optional<DriveInfo> GetIOObjectDriveInfo(
    io_object_t io_object);
#endif

}  // namespace base

#endif  // BASE_FILES_DRIVE_INFO_H_
