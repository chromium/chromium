// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_system_provider/operations/open_file.h"

#include "chrome/common/extensions/api/file_system_provider.h"
#include "chrome/common/extensions/api/file_system_provider_internal.h"

namespace ash::file_system_provider::operations {

namespace {

// Extracts out the `cloud_file_info` and `size` from the `OpenFile` success
// params. Currently only the downstream `CloudFileSystem` cares about the
// `cloud_file_info` and `size` so only extract that information in (if it
// exists).
std::unique_ptr<EntryMetadata> GetEntryMetadataFromParams(
    const extensions::api::file_system_provider_internal::
        OpenFileRequestedSuccess::Params* params) {
  std::unique_ptr<EntryMetadata> metadata = std::make_unique<EntryMetadata>();
  if (params && params->metadata.has_value()) {
    if (params->metadata->cloud_file_info.has_value() &&
        params->metadata->cloud_file_info->version_tag.has_value()) {
      metadata->cloud_file_info = std::make_unique<CloudFileInfo>(
          params->metadata->cloud_file_info->version_tag.value());
    }
    if (params->metadata->size.has_value()) {
      metadata->size = std::make_unique<int64_t>(
          static_cast<int64_t>(*params->metadata->size));
    }
  }
  return metadata;
}

}  // namespace

OpenFile::OpenFile(RequestDispatcher* dispatcher,
                   const ProvidedFileSystemInfo& file_system_info,
                   const base::FilePath& file_path,
                   OpenFileMode mode,
                   ProvidedFileSystemInterface::OpenFileCallback callback)
    : Operation(dispatcher, file_system_info),
      file_path_(file_path),
      mode_(mode),
      callback_(std::move(callback)) {}

OpenFile::~OpenFile() = default;

bool OpenFile::Execute(int request_id) {
  using extensions::api::file_system_provider::OpenFileRequestedOptions;

  if (!file_system_info_.writable() && mode_ == OPEN_FILE_MODE_WRITE) {
    return false;
  }

  OpenFileRequestedOptions options;
  options.file_system_id = file_system_info_.file_system_id();
  options.request_id = request_id;
  options.file_path = file_path_.AsUTF8Unsafe();

  switch (mode_) {
    case OPEN_FILE_MODE_READ:
      options.mode = extensions::api::file_system_provider::OpenFileMode::kRead;
      break;
    case OPEN_FILE_MODE_WRITE:
      options.mode =
          extensions::api::file_system_provider::OpenFileMode::kWrite;
      break;
  }

  return SendEvent(
      request_id,
      extensions::events::FILE_SYSTEM_PROVIDER_ON_OPEN_FILE_REQUESTED,
      extensions::api::file_system_provider::OnOpenFileRequested::kEventName,
      extensions::api::file_system_provider::OnOpenFileRequested::Create(
          options));
}

void OpenFile::OnSuccess(int request_id,
                         const RequestValue& result,
                         bool has_more) {
  // File handle is the same as request id of the OpenFile operation.
  DCHECK(callback_);

  std::move(callback_).Run(
      request_id, base::File::FILE_OK,
      GetEntryMetadataFromParams(result.open_file_success_params()));
}

void OpenFile::OnError(/*request_id=*/int,
                       /*result=*/const RequestValue&,
                       base::File::Error error) {
  DCHECK(callback_);
  std::move(callback_).Run(/*file_handle=*/0, error,
                           /*cloud_file_info=*/nullptr);
}

}  // namespace ash::file_system_provider::operations
