// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_FILE_H_
#define ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_FILE_H_

#include "ash/public/cpp/ash_public_export.h"
#include "base/files/file_path.h"
#include "url/gurl.h"

namespace ash {

// Representation of a file backing a holding space item.
struct ASH_PUBLIC_EXPORT HoldingSpaceFile {
  // Enumeration of file system types corresponding to
  // `storage::FileSystemType`. These values are persisted to logs. Entries
  // should not be renumbered and numeric values should never be reused.
  enum class FileSystemType {
    kMinValue = 0,
    kUnknown = kMinValue,
    kTemporary = 1,
    kPersistent = 2,
    kIsolated = 3,
    kExternal = 4,
    kTest = 5,
    kLocal = 6,
    // kRestrictedLocal = 7,
    kDragged = 8,
    kLocalMedia = 9,
    kDeviceMedia = 10,
    kSyncable = 11,
    kSyncableForInternalSync = 12,
    kLocalForPlatformApp = 13,
    kForTransientFile = 14,
    kProvided = 15,
    kDeviceMediaAsFileStorage = 16,
    kArcContent = 17,
    kArcDocumentsProvider = 18,
    kDriveFs = 19,
    kSmbFs = 20,
    kFuseBox = 21,
    kMaxValue = kFuseBox,
  };

  HoldingSpaceFile(const base::FilePath& file_path,
                   FileSystemType file_system_type,
                   const GURL& file_system_url);

  HoldingSpaceFile(const HoldingSpaceFile&);
  HoldingSpaceFile(HoldingSpaceFile&&);
  HoldingSpaceFile& operator=(const HoldingSpaceFile&);
  HoldingSpaceFile& operator=(HoldingSpaceFile&&);
  ~HoldingSpaceFile();

  bool operator==(const HoldingSpaceFile&) const;
  bool operator!=(const HoldingSpaceFile&) const;

  base::FilePath file_path;
  FileSystemType file_system_type;
  GURL file_system_url;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_FILE_H_
