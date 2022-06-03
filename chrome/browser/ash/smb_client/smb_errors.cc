// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/smb_client/smb_errors.h"

#include "base/logging.h"

namespace ash {
namespace smb_client {

base::File::Error TranslateToFileError(smbprovider::ErrorType error) {
  DCHECK_NE(smbprovider::ERROR_NONE, error);

  switch (error) {
    case smbprovider::ERROR_OK:
      return base::File::FILE_OK;
    case smbprovider::ERROR_FAILED:
      return base::File::FILE_ERROR_FAILED;
    case smbprovider::ERROR_IN_USE:
      return base::File::FILE_ERROR_IN_USE;
    case smbprovider::ERROR_EXISTS:
      return base::File::FILE_ERROR_EXISTS;
    case smbprovider::ERROR_NOT_FOUND:
      return base::File::FILE_ERROR_NOT_FOUND;
    case smbprovider::ERROR_ACCESS_DENIED:
      return base::File::FILE_ERROR_ACCESS_DENIED;
    case smbprovider::ERROR_TOO_MANY_OPENED:
      return base::File::FILE_ERROR_TOO_MANY_OPENED;
    case smbprovider::ERROR_NO_MEMORY:
      return base::File::FILE_ERROR_NO_MEMORY;
    case smbprovider::ERROR_NO_SPACE:
      return base::File::FILE_ERROR_NO_SPACE;
    case smbprovider::ERROR_NOT_A_DIRECTORY:
      return base::File::FILE_ERROR_NOT_A_DIRECTORY;
    case smbprovider::ERROR_INVALID_OPERATION:
      return base::File::FILE_ERROR_INVALID_OPERATION;
    case smbprovider::ERROR_SECURITY:
      return base::File::FILE_ERROR_SECURITY;
    case smbprovider::ERROR_ABORT:
      return base::File::FILE_ERROR_ABORT;
    case smbprovider::ERROR_NOT_A_FILE:
      return base::File::FILE_ERROR_NOT_A_FILE;
    case smbprovider::ERROR_NOT_EMPTY:
      return base::File::FILE_ERROR_NOT_EMPTY;
    case smbprovider::ERROR_INVALID_URL:
      return base::File::FILE_ERROR_INVALID_URL;
    case smbprovider::ERROR_IO:
      return base::File::FILE_ERROR_IO;
    case smbprovider::ERROR_DBUS_PARSE_FAILED:
      // DBUS_PARSE_FAILED maps to generic ERROR_FAILED
      LOG(ERROR) << "DBUS PARSE FAILED";
      return base::File::FILE_ERROR_FAILED;
    default:
      break;
  }

  NOTREACHED();
  return base::File::FILE_ERROR_FAILED;
}

smbprovider::ErrorType TranslateToErrorType(base::File::Error error) {
  switch (error) {
    case base::File::FILE_OK:
      return smbprovider::ERROR_OK;
    case base::File::FILE_ERROR_FAILED:
      return smbprovider::ERROR_FAILED;
    case base::File::FILE_ERROR_IN_USE:
      return smbprovider::ERROR_IN_USE;
    case base::File::FILE_ERROR_EXISTS:
      return smbprovider::ERROR_EXISTS;
    case base::File::FILE_ERROR_NOT_FOUND:
      return smbprovider::ERROR_NOT_FOUND;
    case base::File::FILE_ERROR_ACCESS_DENIED:
      return smbprovider::ERROR_ACCESS_DENIED;
    case base::File::FILE_ERROR_TOO_MANY_OPENED:
      return smbprovider::ERROR_TOO_MANY_OPENED;
    case base::File::FILE_ERROR_NO_MEMORY:
      return smbprovider::ERROR_NO_MEMORY;
    case base::File::FILE_ERROR_NO_SPACE:
      return smbprovider::ERROR_NO_SPACE;
    case base::File::FILE_ERROR_NOT_A_DIRECTORY:
      return smbprovider::ERROR_NOT_A_DIRECTORY;
    case base::File::FILE_ERROR_INVALID_OPERATION:
      return smbprovider::ERROR_INVALID_OPERATION;
    case base::File::FILE_ERROR_SECURITY:
      return smbprovider::ERROR_SECURITY;
    case base::File::FILE_ERROR_ABORT:
      return smbprovider::ERROR_ABORT;
    case base::File::FILE_ERROR_NOT_A_FILE:
      return smbprovider::ERROR_NOT_A_FILE;
    case base::File::FILE_ERROR_NOT_EMPTY:
      return smbprovider::ERROR_NOT_EMPTY;
    case base::File::FILE_ERROR_INVALID_URL:
      return smbprovider::ERROR_INVALID_URL;
    case base::File::FILE_ERROR_IO:
      return smbprovider::ERROR_IO;
    default:
      break;
  }

  NOTREACHED();
  return smbprovider::ERROR_NONE;
}

SmbMountResult TranslateErrorToMountResult(smbprovider::ErrorType error) {
  DCHECK_NE(smbprovider::ERROR_NONE, error);

  switch (error) {
    case smbprovider::ERROR_OK:
      return SmbMountResult::kSuccess;
    case smbprovider::ERROR_EXISTS:
    case smbprovider::ERROR_IN_USE:
      return SmbMountResult::kMountExists;
    case smbprovider::ERROR_NOT_FOUND:
    case smbprovider::ERROR_NOT_A_DIRECTORY:
      return SmbMountResult::kNotFound;
    case smbprovider::ERROR_ACCESS_DENIED:
    case smbprovider::ERROR_SECURITY:
      return SmbMountResult::kAuthenticationFailed;
    case smbprovider::ERROR_SMB1_UNSUPPORTED:
      return SmbMountResult::kUnsupportedDevice;
    case smbprovider::ERROR_INVALID_URL:
      return SmbMountResult::kInvalidUrl;
    case smbprovider::ERROR_IO:
      return SmbMountResult::kIoError;
    case smbprovider::ERROR_TOO_MANY_OPENED:
      return SmbMountResult::kTooManyOpened;
    case smbprovider::ERROR_NO_MEMORY:
    case smbprovider::ERROR_NO_SPACE:
      return SmbMountResult::kOutOfMemory;
    case smbprovider::ERROR_INVALID_OPERATION:
      return SmbMountResult::kInvalidOperation;
    case smbprovider::ERROR_ABORT:
      return SmbMountResult::kAborted;
    case smbprovider::ERROR_DBUS_PARSE_FAILED:
      return SmbMountResult::kDbusParseFailed;
    case smbprovider::ERROR_NOT_A_FILE:
    case smbprovider::ERROR_NOT_EMPTY:
    case smbprovider::ERROR_FAILED:
      return SmbMountResult::kUnknownFailure;
    default:
      break;
  }

  NOTREACHED();
  return SmbMountResult::kUnknownFailure;
}

SmbMountResult TranslateErrorToMountResult(base::File::Error error) {
  return TranslateErrorToMountResult(TranslateToErrorType(error));
}

}  // namespace smb_client
}  // namespace ash
