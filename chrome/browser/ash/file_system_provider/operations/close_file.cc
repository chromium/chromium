// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_system_provider/operations/close_file.h"

#include <string>

#include "chrome/common/extensions/api/file_system_provider.h"
#include "chrome/common/extensions/api/file_system_provider_internal.h"

namespace ash::file_system_provider::operations {

CloseFile::CloseFile(RequestDispatcher* dispatcher,
                     const ProvidedFileSystemInfo& file_system_info,
                     int open_request_id,
                     storage::AsyncFileUtil::StatusCallback callback)
    : Operation(dispatcher, file_system_info),
      open_request_id_(open_request_id),
      callback_(std::move(callback)) {}

CloseFile::~CloseFile() = default;

bool CloseFile::Execute(int request_id) {
  using extensions::api::file_system_provider::CloseFileRequestedOptions;

  CloseFileRequestedOptions options;
  options.file_system_id = file_system_info_.file_system_id();
  options.request_id = request_id;
  options.open_request_id = open_request_id_;

  return SendEvent(
      request_id,
      extensions::events::FILE_SYSTEM_PROVIDER_ON_CLOSE_FILE_REQUESTED,
      extensions::api::file_system_provider::OnCloseFileRequested::kEventName,
      extensions::api::file_system_provider::OnCloseFileRequested::Create(
          options));
}

void CloseFile::OnSuccess(/*request_id=*/int,
                          const RequestValue& result,
                          bool has_more) {
  std::move(callback_).Run(base::File::FILE_OK);
}

void CloseFile::OnError(/*request_id=*/int,
                        /*result=*/const RequestValue&,
                        base::File::Error error) {
  std::move(callback_).Run(error);
}

}  // namespace ash::file_system_provider::operations
