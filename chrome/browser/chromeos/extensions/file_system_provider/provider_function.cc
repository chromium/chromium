// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "chrome/browser/ash/file_system_provider/provided_file_system_interface.h"
#include "chrome/browser/ash/file_system_provider/request_manager.h"
#include "chrome/browser/ash/file_system_provider/request_value.h"
#include "chrome/browser/ash/file_system_provider/service.h"
#include "chrome/browser/chromeos/extensions/file_system_provider/file_system_provider_api.h"
#include "chrome/common/extensions/api/file_system_provider_internal.h"

using ash::file_system_provider::ProvidedFileSystemInterface;
using ash::file_system_provider::ProviderId;
using ash::file_system_provider::RequestManager;
using ash::file_system_provider::RequestValue;
using ash::file_system_provider::Service;

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

FileSystemProviderInternalFunction::FileSystemProviderInternalFunction()
    : request_id_(0), request_manager_(NULL) {
}

ExtensionFunction::ResponseAction
FileSystemProviderInternalFunction::RejectRequest(
    std::unique_ptr<RequestValue> value,
    base::File::Error error) {
  const base::File::Error result =
      request_manager_->RejectRequest(request_id_, std::move(value), error);
  if (result != base::File::FILE_OK)
    return RespondNow(Error(FileErrorToString(result)));
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
FileSystemProviderInternalFunction::FulfillRequest(
    std::unique_ptr<RequestValue> value,
    bool has_more) {
  const base::File::Error result =
      request_manager_->FulfillRequest(request_id_, std::move(value), has_more);
  if (result != base::File::FILE_OK)
    return RespondNow(Error(FileErrorToString(result)));
  return RespondNow(NoArguments());
}

bool FileSystemProviderInternalFunction::PreRunValidation(std::string* error) {
  if (!ExtensionFunction::PreRunValidation(error))
    return false;

  EXTENSION_FUNCTION_PRERUN_VALIDATE(args().size() >= 2);

  const auto& file_system_id_value = args()[0];
  const auto& request_id_value = args()[1];
  EXTENSION_FUNCTION_PRERUN_VALIDATE(file_system_id_value.is_string());
  EXTENSION_FUNCTION_PRERUN_VALIDATE(request_id_value.is_int());
  std::string file_system_id = file_system_id_value.GetString();
  request_id_ = request_id_value.GetInt();

  Service* service = Service::Get(browser_context());
  if (!service) {
    *error = "File system provider service not found.";
    return false;
  }

  ProvidedFileSystemInterface* file_system = service->GetProvidedFileSystem(
      ProviderId::CreateFromExtensionId(extension_id()), file_system_id);
  if (!file_system) {
    *error = FileErrorToString(base::File::FILE_ERROR_NOT_FOUND);
    return false;
  }

  request_manager_ = file_system->GetRequestManager();
  return true;
}

}  // namespace extensions
