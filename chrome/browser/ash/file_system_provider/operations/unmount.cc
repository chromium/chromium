// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_system_provider/operations/unmount.h"

#include "base/values.h"
#include "chrome/common/extensions/api/file_system_provider.h"

namespace ash::file_system_provider::operations {

Unmount::Unmount(RequestDispatcher* dispatcher,
                 const ProvidedFileSystemInfo& file_system_info,
                 storage::AsyncFileUtil::StatusCallback callback)
    : Operation(dispatcher, file_system_info), callback_(std::move(callback)) {}

Unmount::~Unmount() = default;

bool Unmount::Execute(int request_id) {
  using extensions::api::file_system_provider::UnmountRequestedOptions;

  UnmountRequestedOptions options;
  options.file_system_id = file_system_info_.file_system_id();
  options.request_id = request_id;

  return SendEvent(
      request_id, extensions::events::FILE_SYSTEM_PROVIDER_ON_UNMOUNT_REQUESTED,
      extensions::api::file_system_provider::OnUnmountRequested::kEventName,
      extensions::api::file_system_provider::OnUnmountRequested::Create(
          options));
}

void Unmount::OnSuccess(/*request_id=*/int,
                        /*result=*/const RequestValue&,
                        /*has_more=*/bool) {
  DCHECK(callback_);
  std::move(callback_).Run(base::File::FILE_OK);
}

void Unmount::OnError(/*request_id=*/int,
                      /*result=*/const RequestValue&,
                      base::File::Error error) {
  DCHECK(callback_);
  std::move(callback_).Run(error);
}

}  // namespace ash::file_system_provider::operations
