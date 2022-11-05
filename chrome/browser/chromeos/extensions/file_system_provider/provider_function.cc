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
      return extensions::api::file_system_provider::PROVIDER_ERROR_OK;
    case base::File::FILE_ERROR_FAILED:
      return extensions::api::file_system_provider::PROVIDER_ERROR_FAILED;
    case base::File::FILE_ERROR_IN_USE:
      return extensions::api::file_system_provider::PROVIDER_ERROR_IN_USE;
    case base::File::FILE_ERROR_EXISTS:
      return extensions::api::file_system_provider::PROVIDER_ERROR_EXISTS;
    case base::File::FILE_ERROR_NOT_FOUND:
      return extensions::api::file_system_provider::PROVIDER_ERROR_NOT_FOUND;
    case base::File::FILE_ERROR_ACCESS_DENIED:
      return extensions::api::file_system_provider::
          PROVIDER_ERROR_ACCESS_DENIED;
    case base::File::FILE_ERROR_TOO_MANY_OPENED:
      return extensions::api::file_system_provider::
          PROVIDER_ERROR_TOO_MANY_OPENED;
    case base::File::FILE_ERROR_NO_MEMORY:
      return extensions::api::file_system_provider::PROVIDER_ERROR_NO_MEMORY;
    case base::File::FILE_ERROR_NO_SPACE:
      return extensions::api::file_system_provider::PROVIDER_ERROR_NO_SPACE;
    case base::File::FILE_ERROR_NOT_A_DIRECTORY:
      return extensions::api::file_system_provider::
          PROVIDER_ERROR_NOT_A_DIRECTORY;
    case base::File::FILE_ERROR_INVALID_OPERATION:
      return extensions::api::file_system_provider::
          PROVIDER_ERROR_INVALID_OPERATION;
    case base::File::FILE_ERROR_SECURITY:
      return extensions::api::file_system_provider::PROVIDER_ERROR_SECURITY;
    case base::File::FILE_ERROR_ABORT:
      return extensions::api::file_system_provider::PROVIDER_ERROR_ABORT;
    case base::File::FILE_ERROR_NOT_A_FILE:
      return extensions::api::file_system_provider::PROVIDER_ERROR_NOT_A_FILE;
    case base::File::FILE_ERROR_NOT_EMPTY:
      return extensions::api::file_system_provider::PROVIDER_ERROR_NOT_EMPTY;
    case base::File::FILE_ERROR_INVALID_URL:
      return extensions::api::file_system_provider::PROVIDER_ERROR_INVALID_URL;
    case base::File::FILE_ERROR_IO:
      return extensions::api::file_system_provider::PROVIDER_ERROR_IO;
    case base::File::FILE_ERROR_MAX:
      NOTREACHED();
  }

  return extensions::api::file_system_provider::PROVIDER_ERROR_FAILED;
}

}  // namespace

namespace extensions {

base::File::Error ProviderErrorToFileError(
    api::file_system_provider::ProviderError error) {
  switch (error) {
    case api::file_system_provider::PROVIDER_ERROR_OK:
      return base::File::FILE_OK;
    case api::file_system_provider::PROVIDER_ERROR_FAILED:
      return base::File::FILE_ERROR_FAILED;
    case api::file_system_provider::PROVIDER_ERROR_IN_USE:
      return base::File::FILE_ERROR_IN_USE;
    case api::file_system_provider::PROVIDER_ERROR_EXISTS:
      return base::File::FILE_ERROR_EXISTS;
    case api::file_system_provider::PROVIDER_ERROR_NOT_FOUND:
      return base::File::FILE_ERROR_NOT_FOUND;
    case api::file_system_provider::PROVIDER_ERROR_ACCESS_DENIED:
      return base::File::FILE_ERROR_ACCESS_DENIED;
    case api::file_system_provider::PROVIDER_ERROR_TOO_MANY_OPENED:
      return base::File::FILE_ERROR_TOO_MANY_OPENED;
    case api::file_system_provider::PROVIDER_ERROR_NO_MEMORY:
      return base::File::FILE_ERROR_NO_MEMORY;
    case api::file_system_provider::PROVIDER_ERROR_NO_SPACE:
      return base::File::FILE_ERROR_NO_SPACE;
    case api::file_system_provider::PROVIDER_ERROR_NOT_A_DIRECTORY:
      return base::File::FILE_ERROR_NOT_A_DIRECTORY;
    case api::file_system_provider::PROVIDER_ERROR_INVALID_OPERATION:
      return base::File::FILE_ERROR_INVALID_OPERATION;
    case api::file_system_provider::PROVIDER_ERROR_SECURITY:
      return base::File::FILE_ERROR_SECURITY;
    case api::file_system_provider::PROVIDER_ERROR_ABORT:
      return base::File::FILE_ERROR_ABORT;
    case api::file_system_provider::PROVIDER_ERROR_NOT_A_FILE:
      return base::File::FILE_ERROR_NOT_A_FILE;
    case api::file_system_provider::PROVIDER_ERROR_NOT_EMPTY:
      return base::File::FILE_ERROR_NOT_EMPTY;
    case api::file_system_provider::PROVIDER_ERROR_INVALID_URL:
      return base::File::FILE_ERROR_INVALID_URL;
    case api::file_system_provider::PROVIDER_ERROR_IO:
      return base::File::FILE_ERROR_IO;
    case api::file_system_provider::PROVIDER_ERROR_NONE:
      NOTREACHED();
  }

  return base::File::FILE_ERROR_FAILED;
}

std::string FileErrorToString(base::File::Error error) {
  return extensions::api::file_system_provider::ToString(
      FileErrorToProviderError(error));
}

}  // namespace extensions
