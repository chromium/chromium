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

// Sets a reparse point. `source` will now point to `target`. Returns true if
// the call succeeds, false otherwise.
bool FilePathReparsePoint::SetReparsePoint(HANDLE source,
                                           const FilePath& target_path) {
  std::wstring kPathPrefix = FILE_PATH_LITERAL("\\??\\");
  std::wstring target_str;
  // The junction will not work if the target path does not start with \??\ .
  if (kPathPrefix != target_path.value().substr(0, kPathPrefix.size())) {
    target_str += kPathPrefix;
  }
  target_str += target_path.value();
  alignas(REPARSE_DATA_BUFFER) uint8_t buffer[2000] = {};
  DWORD returned;

  constexpr size_t kPathBufferOffset =
      offsetof(REPARSE_DATA_BUFFER, MountPointReparseBuffer.PathBuffer);
  auto dest_span = base::span(buffer).subspan(kPathBufferOffset);

  REPARSE_DATA_BUFFER* data = reinterpret_cast<REPARSE_DATA_BUFFER*>(buffer);
  data->ReparseTag = IO_REPARSE_TAG_MOUNT_POINT;
  auto target_str_span = base::as_byte_span(target_str);
  dest_span.copy_prefix_from(target_str_span);
  std::ranges::fill(dest_span.subspan(target_str_span.size(), 2u), 0);

  // Note: Lengths are in bytes and should NOT include the null terminator
  // for the SubstituteNameLength, but the buffer itself must contain it.
  data->MountPointReparseBuffer.SubstituteNameOffset = 0;
  data->MountPointReparseBuffer.SubstituteNameLength = target_str_span.size();
  data->MountPointReparseBuffer.PrintNameOffset =
      target_str_span.size() + sizeof(wchar_t);
  data->MountPointReparseBuffer.PrintNameLength = 0;

  // ReparseDataLength is the size of the MountPointReparseBuffer data
  // (starting from SubstituteNameOffset) plus the PathBuffer data used,
  // including the null terminator.
  data->ReparseDataLength =
      static_cast<USHORT>(target_str_span.size() + sizeof(wchar_t) + 10);

  // Total buffer size is the header (8 bytes) + ReparseDataLength.
  int total_data_size = data->ReparseDataLength + 8;

  return ::DeviceIoControl(source, FSCTL_SET_REPARSE_POINT, buffer,
                           total_data_size, nullptr, 0, &returned,
                           nullptr) != 0;
}

// Delete the reparse point referenced by |source|. Returns true if the call
// succeeds, false otherwise.
bool FilePathReparsePoint::DeleteReparsePoint(HANDLE source) {
  DWORD returned;
  REPARSE_DATA_BUFFER data = {0};
  data.ReparseTag = IO_REPARSE_TAG_MOUNT_POINT;
  return ::DeviceIoControl(source, FSCTL_DELETE_REPARSE_POINT, &data, 8, NULL,
                           0, &returned, NULL) != 0;
}

}  // namespace base::test
