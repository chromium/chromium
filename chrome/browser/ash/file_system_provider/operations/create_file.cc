// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_system_provider/operations/create_file.h"

#include <string>

#include "chrome/common/extensions/api/file_system_provider.h"
#include "chrome/common/extensions/api/file_system_provider_internal.h"

namespace ash::file_system_provider::operations {

CreateFile::CreateFile(RequestDispatcher* dispatcher,
                       const ProvidedFileSystemInfo& file_system_info,
                       const base::FilePath& file_path,
                       storage::AsyncFileUtil::StatusCallback callback)
    : Operation(dispatcher, file_system_info),
      file_path_(file_path),
      callback_(std::move(callback)) {}

CreateFile::~CreateFile() = default;

bool CreateFile::Execute(int request_id) {
  using extensions::api::file_system_provider::CreateFileRequestedOptions;

  if (!file_system_info_.writable())
    return false;

  CreateFileRequestedOptions options;
  options.file_system_id = file_system_info_.file_system_id();
  options.request_id = request_id;
  options.file_path = file_path_.AsUTF8Unsafe();

  return SendEvent(
      request_id,
      extensions::events::FILE_SYSTEM_PROVIDER_ON_CREATE_FILE_REQUESTED,
      extensions::api::file_system_provider::OnCreateFileRequested::kEventName,
      extensions::api::file_system_provider::OnCreateFileRequested::Create(
          options));
}

void CreateFile::OnSuccess(/*request_id=*/int,
                           /*result=*/const RequestValue&,
                           bool has_more) {
  DCHECK(callback_);
  std::move(callback_).Run(base::File::FILE_OK);
}

void CreateFile::OnError(/*request_id=*/int,
                         /*result=*/const RequestValue&,
                         base::File::Error error) {
  DCHECK(callback_);
  std::move(callback_).Run(error);
}

}  // namespace ash::file_system_provider::operations
