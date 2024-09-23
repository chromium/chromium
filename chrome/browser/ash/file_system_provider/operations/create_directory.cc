// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_system_provider/operations/create_directory.h"

#include <string>

#include "chrome/common/extensions/api/file_system_provider.h"
#include "chrome/common/extensions/api/file_system_provider_internal.h"

namespace ash::file_system_provider::operations {

CreateDirectory::CreateDirectory(
    RequestDispatcher* dispatcher,
    const ProvidedFileSystemInfo& file_system_info,
    const base::FilePath& directory_path,
    bool recursive,
    storage::AsyncFileUtil::StatusCallback callback)
    : Operation(dispatcher, file_system_info),
      directory_path_(directory_path),
      recursive_(recursive),
      callback_(std::move(callback)) {}

CreateDirectory::~CreateDirectory() = default;

bool CreateDirectory::Execute(int request_id) {
  using extensions::api::file_system_provider::CreateDirectoryRequestedOptions;

  if (!file_system_info_.writable())
    return false;

  CreateDirectoryRequestedOptions options;
  options.file_system_id = file_system_info_.file_system_id();
  options.request_id = request_id;
  options.directory_path = directory_path_.AsUTF8Unsafe();
  options.recursive = recursive_;

  return SendEvent(
      request_id,
      extensions::events::FILE_SYSTEM_PROVIDER_ON_CREATE_DIRECTORY_REQUESTED,
      extensions::api::file_system_provider::OnCreateDirectoryRequested::
          kEventName,
      extensions::api::file_system_provider::OnCreateDirectoryRequested::Create(
          options));
}

void CreateDirectory::OnSuccess(/*request_id=*/int,
                                /*result=*/const RequestValue&,
                                bool has_more) {
  DCHECK(callback_);
  std::move(callback_).Run(base::File::FILE_OK);
}

void CreateDirectory::OnError(/*request_id=*/int,
                              /*result=*/const RequestValue&,
                              base::File::Error error) {
  DCHECK(callback_);
  std::move(callback_).Run(error);
}

}  // namespace ash::file_system_provider::operations
