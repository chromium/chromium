// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/file_path_reparse_point_win.h"

#include <windows.h>

#include <winioctl.h>

#include <utility>

namespace base::test {

namespace {
// https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntifs/ns-ntifs-_reparse_data_buffer
// This struct should be available in the windows sdk in ntifs.h, but the
// chromium builders do not find this file.
typedef struct _REPARSE_DATA_BUFFER {
  ULONG ReparseTag;
  USHORT ReparseDataLength;
  USHORT Reserved;
  union {
    struct {
      USHORT SubstituteNameOffset;
      USHORT SubstituteNameLength;
      USHORT PrintNameOffset;
      USHORT PrintNameLength;
      ULONG Flags;
      WCHAR PathBuffer[1];
    } SymbolicLinkReparseBuffer;
    struct {
      USHORT SubstituteNameOffset;
      USHORT SubstituteNameLength;
      USHORT PrintNameOffset;
      USHORT PrintNameLength;
      WCHAR PathBuffer[1];
    } MountPointReparseBuffer;
    struct {
      UCHAR DataBuffer[1];
    } GenericReparseBuffer;
  };
} REPARSE_DATA_BUFFER, *PREPARSE_DATA_BUFFER;
}  // namespace

std::optional<base::test::FilePathReparsePoint> FilePathReparsePoint::Create(
    const FilePath& source,
    const FilePath& target) {
  auto reparse_point = base::test::FilePathReparsePoint(source, target);
  if (!reparse_point.IsValid()) {
    return std::nullopt;
  }
  return std::move(reparse_point);
}

FilePathReparsePoint::FilePathReparsePoint(FilePathReparsePoint&& other)
    : dir_(std::move(other.dir_)),
      created_(std::exchange(other.created_, false)) {}

FilePathReparsePoint& FilePathReparsePoint::operator=(
    FilePathReparsePoint&& other) {
  dir_ = std::move(other.dir_);
  created_ = std::exchange(other.created_, false);
  return *this;
}

FilePathReparsePoint::~FilePathReparsePoint() {
  if (created_) {
    DeleteReparsePoint(dir_.get());
  }
}

// Creates a reparse point from |source| (an empty directory) to |target|.
FilePathReparsePoint::FilePathReparsePoint(const FilePath& source,
                                           const FilePath& target) {
  dir_.Set(
      ::CreateFile(source.value().c_str(), GENERIC_READ | GENERIC_WRITE,
                   FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
                   OPEN_EXISTING,
                   FILE_FLAG_BACKUP_SEMANTICS,  // Needed to open a directory.
                   NULL));
  created_ = dir_.is_valid() && SetReparsePoint(dir_.get(), target);
}

// Sets a reparse point. |source| will now point to |target|. Returns true if
// the call succeeds, false otherwise.
bool FilePathReparsePoint::SetReparsePoint(HANDLE source,
                                           const FilePath& target_path) {
  std::wstring kPathPrefix = FILE_PATH_LITERAL("\\??\\");
  std::wstring target_str;
  // The juction will not work if the target path does not start with \??\ .
  if (kPathPrefix != target_path.value().substr(0, kPathPrefix.size())) {
    target_str += kPathPrefix;
  }
  target_str += target_path.value();
  const wchar_t* target = target_str.c_str();
  USHORT size_target = static_cast<USHORT>(wcslen(target)) * sizeof(target[0]);
  char buffer[2000] = {0};
  DWORD returned;

  REPARSE_DATA_BUFFER* data = reinterpret_cast<REPARSE_DATA_BUFFER*>(buffer);

  data->ReparseTag = 0xa0000003;
  memcpy(data->MountPointReparseBuffer.PathBuffer, target, size_target + 2);

  data->MountPointReparseBuffer.SubstituteNameLength = size_target;
  data->MountPointReparseBuffer.PrintNameOffset = size_target + 2;
  data->ReparseDataLength = size_target + 4 + 8;

  int data_size = data->ReparseDataLength + 8;

  if (!DeviceIoControl(source, FSCTL_SET_REPARSE_POINT, &buffer, data_size,
                       NULL, 0, &returned, NULL)) {
    return false;
  }
  return true;
}

// Delete the reparse point referenced by |source|. Returns true if the call
// succeeds, false otherwise.
bool FilePathReparsePoint::DeleteReparsePoint(HANDLE source) {
  DWORD returned;
  REPARSE_DATA_BUFFER data = {0};
  data.ReparseTag = 0xa0000003;
  if (!DeviceIoControl(source, FSCTL_DELETE_REPARSE_POINT, &data, 8, NULL, 0,
                       &returned, NULL)) {
    return false;
  }
  return true;
}

}  // namespace base::test
