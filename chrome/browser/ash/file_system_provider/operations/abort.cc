// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_system_provider/operations/abort.h"

#include <string>

#include "chrome/common/extensions/api/file_system_provider.h"
#include "chrome/common/extensions/api/file_system_provider_internal.h"

namespace ash::file_system_provider::operations {

Abort::Abort(RequestDispatcher* dispatcher,
             const ProvidedFileSystemInfo& file_system_info,
             int operation_request_id,
             storage::AsyncFileUtil::StatusCallback callback)
    : Operation(dispatcher, file_system_info),
      operation_request_id_(operation_request_id),
      callback_(std::move(callback)) {}

Abort::~Abort() = default;

bool Abort::Execute(int request_id) {
  using extensions::api::file_system_provider::AbortRequestedOptions;

  AbortRequestedOptions options;
  options.file_system_id = file_system_info_.file_system_id();
  options.request_id = request_id;
  options.operation_request_id = operation_request_id_;

  return SendEvent(
      request_id, extensions::events::FILE_SYSTEM_PROVIDER_ON_ABORT_REQUESTED,
      extensions::api::file_system_provider::OnAbortRequested::kEventName,
      extensions::api::file_system_provider::OnAbortRequested::Create(options));
}

void Abort::OnSuccess(/*request_id=*/int,
                      /*result=*/const RequestValue&,
                      bool has_more) {
  DCHECK(callback_);
  std::move(callback_).Run(base::File::FILE_OK);
}

void Abort::OnError(/*request_id=*/int,
                    /*result=*/const RequestValue&,
                    base::File::Error error) {
  DCHECK(callback_);
  std::move(callback_).Run(error);
}

}  // namespace ash::file_system_provider::operations
