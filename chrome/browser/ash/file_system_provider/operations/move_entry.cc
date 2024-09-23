// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_system_provider/operations/move_entry.h"

#include <string>

#include "chrome/common/extensions/api/file_system_provider.h"
#include "chrome/common/extensions/api/file_system_provider_internal.h"

namespace ash::file_system_provider::operations {

MoveEntry::MoveEntry(RequestDispatcher* dispatcher,
                     const ProvidedFileSystemInfo& file_system_info,
                     const base::FilePath& source_path,
                     const base::FilePath& target_path,
                     storage::AsyncFileUtil::StatusCallback callback)
    : Operation(dispatcher, file_system_info),
      source_path_(source_path),
      target_path_(target_path),
      callback_(std::move(callback)) {}

MoveEntry::~MoveEntry() = default;

bool MoveEntry::Execute(int request_id) {
  using extensions::api::file_system_provider::MoveEntryRequestedOptions;

  if (!file_system_info_.writable())
    return false;

  MoveEntryRequestedOptions options;
  options.file_system_id = file_system_info_.file_system_id();
  options.request_id = request_id;
  options.source_path = source_path_.AsUTF8Unsafe();
  options.target_path = target_path_.AsUTF8Unsafe();

  return SendEvent(
      request_id,
      extensions::events::FILE_SYSTEM_PROVIDER_ON_MOVE_ENTRY_REQUESTED,
      extensions::api::file_system_provider::OnMoveEntryRequested::kEventName,
      extensions::api::file_system_provider::OnMoveEntryRequested::Create(
          options));
}

void MoveEntry::OnSuccess(/*request_id=*/int,
                          /*result=*/const RequestValue&,
                          bool has_more) {
  DCHECK(callback_);
  std::move(callback_).Run(base::File::FILE_OK);
}

void MoveEntry::OnError(/*request_id=*/int,
                        /*result=*/const RequestValue&,
                        base::File::Error error) {
  DCHECK(callback_);
  std::move(callback_).Run(error);
}

}  // namespace ash::file_system_provider::operations
