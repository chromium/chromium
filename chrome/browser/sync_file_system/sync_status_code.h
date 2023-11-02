// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_SYNC_STATUS_CODE_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_SYNC_STATUS_CODE_H_

#include "base/files/file.h"

namespace leveldb {
class Status;
}

namespace sync_file_system {

enum SyncStatusCode {
  SYNC_STATUS_OK = 0,
  SYNC_STATUS_UNKNOWN = -1000,

  // Generic error code which is not specifically related to a specific
  // submodule error code (yet).
  SYNC_STATUS_FAILED = -1001,

  // Basic ones that could be directly mapped to File::Error.
  SYNC_FILE_ERROR_FAILED = -1,
  SYNC_FILE_ERROR_IN_USE = -2,
  SYNC_FILE_ERROR_EXISTS = -3,
  SYNC_FILE_ERROR_NOT_FOUND = -4,
  SYNC_FILE_ERROR_ACCESS_DENIED = -5,
  SYNC_FILE_ERROR_TOO_MANY_OPENED = -6,
  SYNC_FILE_ERROR_NO_MEMORY = -7,
  SYNC_FILE_ERROR_NO_SPACE = -8,
  SYNC_FILE_ERROR_NOT_A_DIRECTORY = -9,
  SYNC_FILE_ERROR_INVALID_OPERATION = -10,
  SYNC_FILE_ERROR_SECURITY = -11,
  SYNC_FILE_ERROR_ABORT = -12,
  SYNC_FILE_ERROR_NOT_A_FILE = -13,
  SYNC_FILE_ERROR_NOT_EMPTY = -14,
  SYNC_FILE_ERROR_INVALID_URL = -15,
  SYNC_FILE_ERROR_IO = -16,

  // Database related errors.
  SYNC_DATABASE_ERROR_NOT_FOUND = -50,
  SYNC_DATABASE_ERROR_CORRUPTION = -51,
  SYNC_DATABASE_ERROR_IO_ERROR = -52,
  SYNC_DATABASE_ERROR_FAILED = -53,

  // Sync specific status code.
  SYNC_STATUS_FILE_BUSY = -100,
  SYNC_STATUS_HAS_CONFLICT = -101,
  SYNC_STATUS_NO_CONFLICT = -102,
  SYNC_STATUS_ABORT = -103,
  SYNC_STATUS_NO_CHANGE_TO_SYNC = -104,
  SYNC_STATUS_SERVICE_TEMPORARILY_UNAVAILABLE = -105,
  SYNC_STATUS_NETWORK_ERROR = -106,
  SYNC_STATUS_AUTHENTICATION_FAILED = -107,
  SYNC_STATUS_UNKNOWN_ORIGIN = -108,
  SYNC_STATUS_NOT_MODIFIED = -109,
  SYNC_STATUS_SYNC_DISABLED = -110,
  SYNC_STATUS_ACCESS_FORBIDDEN = -111,
  SYNC_STATUS_RETRY = -112,
};

const char* SyncStatusCodeToString(SyncStatusCode status);

SyncStatusCode LevelDBStatusToSyncStatusCode(const leveldb::Status& status);

SyncStatusCode FileErrorToSyncStatusCode(base::File::Error file_error);

base::File::Error SyncStatusCodeToFileError(SyncStatusCode status);

}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_SYNC_STATUS_CODE_H_
