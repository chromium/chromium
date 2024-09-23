// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/smb_client/smb_errors.h"

#include "base/logging.h"

namespace ash::smb_client {

SmbMountResult TranslateErrorToMountResult(smbprovider::ErrorType error) {
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
    case smbprovider::ERROR_OPERATION_FAILED:
      return SmbMountResult::kUnknownFailure;
    case smbprovider::ERROR_NONE:
    case smbprovider::ERROR_PROVIDER_ERROR_COUNT:
    case smbprovider::ERROR_COPY_PENDING:
    case smbprovider::ERROR_COPY_FAILED:
    case smbprovider::ERROR_OPERATION_PENDING:
      NOTREACHED_IN_MIGRATION()
          << "Unexpected smbprovider error: " << (int)error;
      return SmbMountResult::kUnknownFailure;
  }
}

}  // namespace ash::smb_client
