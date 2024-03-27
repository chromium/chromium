// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_system_provider/operations/truncate.h"

#include <string>

#include "chrome/common/extensions/api/file_system_provider.h"
#include "chrome/common/extensions/api/file_system_provider_internal.h"

namespace ash::file_system_provider::operations {

Truncate::Truncate(RequestDispatcher* dispatcher,
                   const ProvidedFileSystemInfo& file_system_info,
                   const base::FilePath& file_path,
                   int64_t length,
                   storage::AsyncFileUtil::StatusCallback callback)
    : Operation(dispatcher, file_system_info),
      file_path_(file_path),
      length_(length),
      callback_(std::move(callback)) {}

Truncate::~Truncate() = default;

bool Truncate::Execute(int request_id) {
  using extensions::api::file_system_provider::TruncateRequestedOptions;

  if (!file_system_info_.writable())
    return false;

  TruncateRequestedOptions options;
  options.file_system_id = file_system_info_.file_system_id();
  options.request_id = request_id;
  options.file_path = file_path_.AsUTF8Unsafe();
  options.length = length_;

  return SendEvent(
      request_id,
      extensions::events::FILE_SYSTEM_PROVIDER_ON_TRUNCATE_REQUESTED,
      extensions::api::file_system_provider::OnTruncateRequested::kEventName,
      extensions::api::file_system_provider::OnTruncateRequested::Create(
          options));
}

void Truncate::OnSuccess(/*request_id=*/int,
                         /*result=*/const RequestValue&,
                         bool has_more) {
  DCHECK(callback_);
  std::move(callback_).Run(base::File::FILE_OK);
}

void Truncate::OnError(/*request_id=*/int,
                       /*result=*/const RequestValue&,
                       base::File::Error error) {
  DCHECK(callback_);
  std::move(callback_).Run(error);
}

}  // namespace ash::file_system_provider::operations
