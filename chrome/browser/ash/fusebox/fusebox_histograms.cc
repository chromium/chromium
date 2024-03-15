// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/fusebox/fusebox_histograms.h"

#include "storage/browser/file_system/file_system_url.h"
#include "storage/common/file_system/file_system_types.h"

namespace fusebox {

HistogramEnumPosixErrorCode GetHistogramEnumPosixErrorCode(
    int posix_error_code) {
  switch (posix_error_code) {
    case 0:
      return HistogramEnumPosixErrorCode::kOK;
    case EFAULT:
      return HistogramEnumPosixErrorCode::kEFAULT;
    case EBUSY:
      return HistogramEnumPosixErrorCode::kEBUSY;
    case EEXIST:
      return HistogramEnumPosixErrorCode::kEEXIST;
    case ENOENT:
      return HistogramEnumPosixErrorCode::kENOENT;
    case EACCES:
      return HistogramEnumPosixErrorCode::kEACCES;
    case EMFILE:
      return HistogramEnumPosixErrorCode::kEMFILE;
    case ENOMEM:
      return HistogramEnumPosixErrorCode::kENOMEM;
    case ENOSPC:
      return HistogramEnumPosixErrorCode::kENOSPC;
    case ENOTDIR:
      return HistogramEnumPosixErrorCode::kENOTDIR;
    case ENOTSUP:
      return HistogramEnumPosixErrorCode::kENOTSUP;
    case EINVAL:
      return HistogramEnumPosixErrorCode::kEINVAL;
    case ENOTEMPTY:
      return HistogramEnumPosixErrorCode::kENOTEMPTY;
    case EIO:
      return HistogramEnumPosixErrorCode::kEIO;
    default:
      break;
  }
  return HistogramEnumPosixErrorCode::kEOTHER;
}

HistogramEnumFileSystemType GetHistogramEnumFileSystemType(
    const storage::FileSystemURL& fs_url) {
  if (fs_url.mount_type() != storage::kFileSystemTypeExternal) {
    return HistogramEnumFileSystemType::kNonExternal;
  }

  switch (fs_url.type()) {
    case storage::kFileSystemTypeLocal:
      return HistogramEnumFileSystemType::kLocal;
    case storage::kFileSystemTypeLocalMedia:
      return HistogramEnumFileSystemType::kLocalMedia;
    case storage::kFileSystemTypeDeviceMedia:
      return HistogramEnumFileSystemType::kDeviceMedia;
    case storage::kFileSystemTypeProvided:
      return HistogramEnumFileSystemType::kProvided;
    case storage::kFileSystemTypeDeviceMediaAsFileStorage:
      return HistogramEnumFileSystemType::kDeviceMediaAsFileStorage;
    case storage::kFileSystemTypeArcContent:
      return HistogramEnumFileSystemType::kArcContent;
    case storage::kFileSystemTypeArcDocumentsProvider:
      return HistogramEnumFileSystemType::kArcDocumentsProvider;
    case storage::kFileSystemTypeDriveFs:
      return HistogramEnumFileSystemType::kDriveFs;
    default:
      break;
  }
  return HistogramEnumFileSystemType::kOther;
}

const char* NameForHistogramEnumFileSystemType(
    const HistogramEnumFileSystemType type) {
  switch (type) {
    case HistogramEnumFileSystemType::kNonExternal:
      return "NonExternal";
    case HistogramEnumFileSystemType::kOther:
      return "Other";
    case HistogramEnumFileSystemType::kLocal:
      return "Local";
    case HistogramEnumFileSystemType::kLocalMedia:
      return "LocalMedia";
    case HistogramEnumFileSystemType::kDeviceMedia:
      return "DeviceMedia";
    case HistogramEnumFileSystemType::kProvided:
      return "Provided";
    case HistogramEnumFileSystemType::kDeviceMediaAsFileStorage:
      return "DeviceMediaAsFileStorage";
    case HistogramEnumFileSystemType::kArcContent:
      return "ArcContent";
    case HistogramEnumFileSystemType::kArcDocumentsProvider:
      return "ArcDocumentsProvider";
    case HistogramEnumFileSystemType::kDriveFs:
      return "DriveFs";
    default:
      break;
  }
  return "Unknown";
}

}  // namespace fusebox
