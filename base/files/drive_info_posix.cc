// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/drive_info.h"

#include <sys/stat.h>
#include <sys/types.h>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include <linux/kdev_t.h>
#endif

namespace base {

std::optional<DriveInfo> GetFileDriveInfo(const FilePath& file_path) {
  DriveInfo drive_info;
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA)
  drive_info.has_seek_penalty = false;
  return drive_info;
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  constexpr char kRotationalFormat[] =
      "/sys/dev/block/%lu:%lu/queue/rotational";
  constexpr char kRemovableFormat[] = "/sys/dev/block/%lu:%lu/removable";
  constexpr char kDeviceFormat[] = "/sys/dev/block/%lu:%lu";
  constexpr char kSizeFormat[] = "/sys/dev/block/%lu:%lu/size";
  File file(file_path, File::FLAG_OPEN | File::FLAG_READ);
  if (!file.IsValid()) {
    return std::nullopt;
  }

  struct stat path_stat;
  int error = File::Fstat(file.GetPlatformFile(), &path_stat);
  if (error < 0) {
    return std::nullopt;
  }

  std::string rotational_path = StringPrintf(
      kRotationalFormat, MAJOR(path_stat.st_dev), MINOR(path_stat.st_dev));
  std::string removable_path = StringPrintf(
      kRemovableFormat, MAJOR(path_stat.st_dev), MINOR(path_stat.st_dev));
  std::string size_path = StringPrintf(kSizeFormat, MAJOR(path_stat.st_dev),
                                       MINOR(path_stat.st_dev));

  // Depending on the device, partitions may not expose the desired info. In the
  // case when reading the device returned by Fstat() fails in both the
  // rotational and removable attributes, attempt to crawl up one directory in
  // the sysfs absolute path for the partition which was queried, which should
  // give the device which contains this partition, and which should contain the
  // desired info in the case where that info is not exposed for the partition
  // itself.
  std::string rotates;
  std::string removable;
  bool rotates_read = ReadFileToString(FilePath(rotational_path), &rotates);
  bool removable_read = ReadFileToString(FilePath(removable_path), &removable);

  if (!rotates_read && !removable_read) {
    std::string device_path = StringPrintf(
        kDeviceFormat, MAJOR(path_stat.st_dev), MINOR(path_stat.st_dev));
    FilePath parent_device_path =
        MakeAbsoluteFilePath(FilePath(device_path)).DirName();
    rotates_read = ReadFileToString(
        parent_device_path.Append("queue/rotational"), &rotates);
    removable_read =
        ReadFileToString(parent_device_path.Append("removable"), &removable);
  }

  if (rotates_read) {
    rotates = TrimString(rotates, "\n", TrimPositions::TRIM_TRAILING);
    if (rotates.length() == 1 && (rotates[0] == '0' || rotates[0] == '1')) {
      drive_info.has_seek_penalty = rotates[0] == '1';
    }
  }
  if (removable_read) {
    removable = TrimString(removable, "\n", TrimPositions::TRIM_TRAILING);
    if (removable.length() == 1 &&
        (removable[0] == '0' || removable[0] == '1')) {
      drive_info.is_removable = removable[0] == '1';
    }
  }

  std::string size;
  uint64_t bytes;
  if (ReadFileToString(FilePath(size_path), &size) &&
      StringToUint64(TrimString(size, "\n", TrimPositions::TRIM_TRAILING),
                     &bytes)) {
    drive_info.size_bytes = bytes;
  }

  return drive_info;
#endif
}

}  // namespace base
