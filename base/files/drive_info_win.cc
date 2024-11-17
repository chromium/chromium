// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/drive_info.h"

#include <windows.h>

#include <winioctl.h>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "build/build_config.h"

namespace base {

DriveInfo::DriveInfo() = default;
DriveInfo::~DriveInfo() = default;
DriveInfo::DriveInfo(DriveInfo&&) = default;
DriveInfo& DriveInfo::operator=(DriveInfo&&) = default;

std::optional<DriveInfo> GetFileDriveInfo(const FilePath& path) {
  std::vector<base::FilePath::StringType> components = path.GetComponents();

  base::File volume(base::FilePath(L"\\\\.\\" + components[0]),
                    base::File::FLAG_OPEN);
  if (!volume.IsValid()) {
    return std::nullopt;
  }
  DriveInfo info;

  STORAGE_PROPERTY_QUERY seek_query = {
      .PropertyId = StorageDeviceSeekPenaltyProperty,
      .QueryType = PropertyStandardQuery};
  DEVICE_SEEK_PENALTY_DESCRIPTOR seek_result;
  DWORD bytes_returned;
  BOOL success =
      DeviceIoControl(volume.GetPlatformFile(), IOCTL_STORAGE_QUERY_PROPERTY,
                      &seek_query, sizeof(seek_query), &seek_result,
                      sizeof(seek_result), &bytes_returned, nullptr);
  if (success == TRUE && bytes_returned >= sizeof(seek_result)) {
    info.has_seek_penalty = seek_result.IncursSeekPenalty != FALSE;
  }

  STORAGE_PROPERTY_QUERY bus_query = {.PropertyId = StorageDeviceProperty,
                                      .QueryType = PropertyStandardQuery};
  STORAGE_DEVICE_DESCRIPTOR bus_result;
  success =
      DeviceIoControl(volume.GetPlatformFile(), IOCTL_STORAGE_QUERY_PROPERTY,
                      &bus_query, sizeof(bus_query), &bus_result,
                      sizeof(bus_result), &bytes_returned, nullptr);
  if (success == TRUE && bytes_returned >= sizeof(bus_result)) {
    info.is_usb = bus_result.BusType == BusTypeUsb;
    info.is_removable = bus_result.RemovableMedia == TRUE;
  }

  PARTITION_INFORMATION_EX partition_info;
  success = DeviceIoControl(
      volume.GetPlatformFile(), IOCTL_DISK_GET_PARTITION_INFO_EX, nullptr, 0,
      &partition_info, sizeof(partition_info), &bytes_returned, nullptr);
  if (success == TRUE && bytes_returned >= sizeof(partition_info)) {
    info.size_bytes = partition_info.PartitionLength.QuadPart;
  }

  return info;
}

}  // namespace base
