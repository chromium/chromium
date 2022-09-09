// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/fusebox/fusebox_errno.h"

#include <errno.h>

#include "base/files/file.h"
#include "storage/common/file_system/file_system_util.h"

namespace fusebox {

int FileErrorToErrno(base::File::Error error) {
  switch (error) {
    case base::File::Error::FILE_OK:
      return 0;
    case base::File::Error::FILE_ERROR_FAILED:
      return EFAULT;
    case base::File::Error::FILE_ERROR_IN_USE:
      return EBUSY;
    case base::File::Error::FILE_ERROR_EXISTS:
      return EEXIST;
    case base::File::Error::FILE_ERROR_NOT_FOUND:
      return ENOENT;
    case base::File::Error::FILE_ERROR_ACCESS_DENIED:
      return EACCES;
    case base::File::Error::FILE_ERROR_TOO_MANY_OPENED:
      return EMFILE;
    case base::File::Error::FILE_ERROR_NO_MEMORY:
      return ENOMEM;
    case base::File::Error::FILE_ERROR_NO_SPACE:
      return ENOSPC;
    case base::File::Error::FILE_ERROR_NOT_A_DIRECTORY:
      return ENOTDIR;
    case base::File::Error::FILE_ERROR_INVALID_OPERATION:
      return ENOTSUP;
    case base::File::Error::FILE_ERROR_SECURITY:
      return EACCES;
    case base::File::Error::FILE_ERROR_ABORT:
      return ENOTSUP;
    case base::File::Error::FILE_ERROR_NOT_A_FILE:
      return EINVAL;
    case base::File::Error::FILE_ERROR_NOT_EMPTY:
      return ENOTEMPTY;
    case base::File::Error::FILE_ERROR_INVALID_URL:
      return EINVAL;
    case base::File::Error::FILE_ERROR_IO:
      return EIO;
    default:
      return EFAULT;
  }
}

int NetErrorToErrno(int error) {
  return FileErrorToErrno(storage::NetErrorToFileError(error));
}

}  // namespace fusebox
