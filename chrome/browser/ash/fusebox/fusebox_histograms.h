// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FUSEBOX_FUSEBOX_HISTOGRAMS_H_
#define CHROME_BROWSER_ASH_FUSEBOX_FUSEBOX_HISTOGRAMS_H_

#include "base/files/file.h"

namespace storage {
class FileSystemURL;
}

namespace fusebox {

// A subset of POSIX error codes (like ENOENT) whose numerical values are
// re-mapped to be stable across platforms (Linux versus Mac, 32-bit versus
// 64-bit, etc).
//
// The subset covers those used by the fusebox::FileErrorToErrno function.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class HistogramEnumPosixErrorCode {
  kUnknown = 0,
  kOK = 1,
  kEOTHER = 2,  // As in, a non-zero EFOO that's not an EBAR listed below.
  kEFAULT = 3,
  kEBUSY = 4,
  kEEXIST = 5,
  kENOENT = 6,
  kEACCES = 7,
  kEMFILE = 8,
  kENOMEM = 9,
  kENOSPC = 10,
  kENOTDIR = 11,
  kENOTSUP = 12,
  kEINVAL = 13,
  kENOTEMPTY = 14,
  kEIO = 15,

  kMaxValue = kEIO,
};

HistogramEnumPosixErrorCode GetHistogramEnumPosixErrorCode(
    int posix_error_code);

// A subset of the storage::FileSystemType enum that is defined in
// storage/common/file_system/file_system_types.h. Importantly, and unlike
// storage::FileSystemType, its numeric values are stable (and thus suitable
// for UMA histograms).
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class HistogramEnumFileSystemType {
  kUnknown = 0,
  kNonExternal = 1,
  kOther = 2,
  kLocal = 3,
  kLocalMedia = 4,
  kDeviceMedia = 5,
  kProvided = 6,
  kDeviceMediaAsFileStorage = 7,
  kArcContent = 8,
  kArcDocumentsProvider = 9,
  kDriveFs = 10,

  kMaxValue = kDriveFs,
};

HistogramEnumFileSystemType GetHistogramEnumFileSystemType(
    const storage::FileSystemURL& fs_url);

const char* NameForHistogramEnumFileSystemType(
    const HistogramEnumFileSystemType type);

}  // namespace fusebox

#endif  // CHROME_BROWSER_ASH_FUSEBOX_FUSEBOX_HISTOGRAMS_H_
