// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "chrome/browser/chromeos/extensions/file_system_provider/file_system_provider_api.h"
#include "chrome/common/extensions/api/file_system_provider_internal.h"

namespace {

// Converts a base::File::Error into the IDL error format.
extensions::api::file_system_provider::ProviderError FileErrorToProviderError(
    base::File::Error error) {
  switch (error) {
    case base::File::FILE_OK:
      return extensions::api::file_system_provider::ProviderError::kOk;
    case base::File::FILE_ERROR_FAILED:
      return extensions::api::file_system_provider::ProviderError::kFailed;
    case base::File::FILE_ERROR_IN_USE:
      return extensions::api::file_system_provider::ProviderError::kInUse;
    case base::File::FILE_ERROR_EXISTS:
      return extensions::api::file_system_provider::ProviderError::kExists;
    case base::File::FILE_ERROR_NOT_FOUND:
      return extensions::api::file_system_provider::ProviderError::kNotFound;
    case base::File::FILE_ERROR_ACCESS_DENIED:
      return extensions::api::file_system_provider::ProviderError::
          kAccessDenied;
    case base::File::FILE_ERROR_TOO_MANY_OPENED:
      return extensions::api::file_system_provider::ProviderError::
          kTooManyOpened;
    case base::File::FILE_ERROR_NO_MEMORY:
      return extensions::api::file_system_provider::ProviderError::kNoMemory;
    case base::File::FILE_ERROR_NO_SPACE:
      return extensions::api::file_system_provider::ProviderError::kNoSpace;
    case base::File::FILE_ERROR_NOT_A_DIRECTORY:
      return extensions::api::file_system_provider::ProviderError::
          kNotADirectory;
    case base::File::FILE_ERROR_INVALID_OPERATION:
      return extensions::api::file_system_provider::ProviderError::
          kInvalidOperation;
    case base::File::FILE_ERROR_SECURITY:
      return extensions::api::file_system_provider::ProviderError::kSecurity;
    case base::File::FILE_ERROR_ABORT:
      return extensions::api::file_system_provider::ProviderError::kAbort;
    case base::File::FILE_ERROR_NOT_A_FILE:
      return extensions::api::file_system_provider::ProviderError::kNotAFile;
    case base::File::FILE_ERROR_NOT_EMPTY:
      return extensions::api::file_system_provider::ProviderError::kNotEmpty;
    case base::File::FILE_ERROR_INVALID_URL:
      return extensions::api::file_system_provider::ProviderError::kInvalidUrl;
    case base::File::FILE_ERROR_IO:
      return extensions::api::file_system_provider::ProviderError::kIo;
    case base::File::FILE_ERROR_MAX:
      NOTREACHED_IN_MIGRATION();
  }

  return extensions::api::file_system_provider::ProviderError::kFailed;
}

}  // namespace

namespace extensions {

base::File::Error ProviderErrorToFileError(
    api::file_system_provider::ProviderError error) {
  switch (error) {
    case api::file_system_provider::ProviderError::kOk:
      return base::File::FILE_OK;
    case api::file_system_provider::ProviderError::kFailed:
      return base::File::FILE_ERROR_FAILED;
    case api::file_system_provider::ProviderError::kInUse:
      return base::File::FILE_ERROR_IN_USE;
    case api::file_system_provider::ProviderError::kExists:
      return base::File::FILE_ERROR_EXISTS;
    case api::file_system_provider::ProviderError::kNotFound:
      return base::File::FILE_ERROR_NOT_FOUND;
    case api::file_system_provider::ProviderError::kAccessDenied:
      return base::File::FILE_ERROR_ACCESS_DENIED;
    case api::file_system_provider::ProviderError::kTooManyOpened:
      return base::File::FILE_ERROR_TOO_MANY_OPENED;
    case api::file_system_provider::ProviderError::kNoMemory:
      return base::File::FILE_ERROR_NO_MEMORY;
    case api::file_system_provider::ProviderError::kNoSpace:
      return base::File::FILE_ERROR_NO_SPACE;
    case api::file_system_provider::ProviderError::kNotADirectory:
      return base::File::FILE_ERROR_NOT_A_DIRECTORY;
    case api::file_system_provider::ProviderError::kInvalidOperation:
      return base::File::FILE_ERROR_INVALID_OPERATION;
    case api::file_system_provider::ProviderError::kSecurity:
      return base::File::FILE_ERROR_SECURITY;
    case api::file_system_provider::ProviderError::kAbort:
      return base::File::FILE_ERROR_ABORT;
    case api::file_system_provider::ProviderError::kNotAFile:
      return base::File::FILE_ERROR_NOT_A_FILE;
    case api::file_system_provider::ProviderError::kNotEmpty:
      return base::File::FILE_ERROR_NOT_EMPTY;
    case api::file_system_provider::ProviderError::kInvalidUrl:
      return base::File::FILE_ERROR_INVALID_URL;
    case api::file_system_provider::ProviderError::kIo:
      return base::File::FILE_ERROR_IO;
    case api::file_system_provider::ProviderError::kNone:
      NOTREACHED_IN_MIGRATION();
  }

  return base::File::FILE_ERROR_FAILED;
}

std::string FileErrorToString(base::File::Error error) {
  return extensions::api::file_system_provider::ToString(
      FileErrorToProviderError(error));
}

}  // namespace extensions
