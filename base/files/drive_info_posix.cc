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
#include "base/strings/stringprintf.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include <linux/kdev_t.h>
#endif

namespace base {

DriveInfo::DriveInfo() = default;
DriveInfo::~DriveInfo() = default;
DriveInfo::DriveInfo(DriveInfo&&) = default;
DriveInfo& DriveInfo::operator=(DriveInfo&&) = default;

std::optional<DriveInfo> GetFileDriveInfo(const FilePath& file_path) {
  DriveInfo drive_info;
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA)
  drive_info.has_seek_penalty = false;
  return drive_info;
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  constexpr char kRotationalFormat[] =
      "/sys/dev/block/%lu:%lu/queue/rotational";
  constexpr char kRemovableFormat[] = "/sys/dev/block/%lu:%lu/removable";
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

  std::string rotates;
  if (ReadFileToString(base::FilePath(rotational_path), &rotates) &&
      rotates.length() == 1 && (rotates[0] == '0' || rotates[0] == '1')) {
    drive_info.has_seek_penalty = rotates[0] == '1';
  }

  std::string removable;
  if (ReadFileToString(base::FilePath(removable_path), &removable) &&
      removable.length() == 1 && (removable[0] == '0' || removable[0] == '1')) {
    drive_info.is_removable = removable[0] == '1';
  }

  std::string size;
  uint64_t bytes;
  if (ReadFileToString(FilePath(size_path), &size) &&
      StringToUint64(size, &bytes)) {
    drive_info.size_bytes = bytes;
  }

  return drive_info;
#endif
}

}  // namespace base
