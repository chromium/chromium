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

}  // namespace fusebox
