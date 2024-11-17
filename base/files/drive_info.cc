// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/drive_info.h"

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_MAC)
#include <IOKit/IOKitLib.h>
#endif

namespace base {

DriveInfo::DriveInfo() = default;
DriveInfo::~DriveInfo() = default;
DriveInfo::DriveInfo(DriveInfo&&) = default;
DriveInfo& DriveInfo::operator=(DriveInfo&&) = default;

#if BUILDFLAG(IS_WIN)
std::optional<DriveInfo> GetFileDriveInfo(const FilePath& path) {
  std::vector<base::FilePath::StringType> components = path.GetComponents();

  base::File volume(base::FilePath(L"\\\\.\\" + components[0]),
                    base::File::FLAG_OPEN);
  if (!volume.IsValid()) {
    return std::nullopt;
  }

  STORAGE_PROPERTY_QUERY seek_query = {
      .PropertyId = StorageDeviceSeekPenaltyProperty,
      .QueryType = PropertyStandardQuery};

  DEVICE_SEEK_PENALTY_DESCRIPTOR seek_result;
  DWORD bytes_returned;

  BOOL success =
      DeviceIoControl(volume.GetPlatformFile(), IOCTL_STORAGE_QUERY_PROPERTY,
                      &seek_query, sizeof(seek_query), &seek_result,
                      sizeof(seek_result), &bytes_returned, nullptr);

  if (success == FALSE || bytes_returned < sizeof(seek_result)) {
    return std::nullopt;
  }

  STORAGE_PROPERTY_QUERY bus_query = {.PropertyId = StorageDeviceProperty,
                                      .QueryType = PropertyStandardQuery};

  STORAGE_DEVICE_DESCRIPTOR bus_result;

  success =
      DeviceIoControl(volume.GetPlatformFile(), IOCTL_STORAGE_QUERY_PROPERTY,
                      &bus_query, sizeof(bus_query), &bus_result,
                      sizeof(bus_result), &bytes_returned, nullptr);

  if (success == FALSE || bytes_returned < sizeof(bus_result)) {
    return std::nullopt;
  }

  DriveInfo info;
  info.has_seek_penalty = seek_result.IncursSeekPenalty != FALSE;
  info.volume_size_bytes = bus_result.Size;
  info.is_usb = bus_result.BusType == BusTypeUsb;
  info.is_removable = bus_result.RemovableMedia == TRUE;
  return info;
}
#endif

}  // namespace base
