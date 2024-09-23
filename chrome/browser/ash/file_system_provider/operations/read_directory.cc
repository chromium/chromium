// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_system_provider/operations/read_directory.h"

#include <stddef.h>

#include <string>
#include <utility>

#include "chrome/browser/ash/file_system_provider/operations/get_metadata.h"
#include "chrome/common/extensions/api/file_system_provider.h"
#include "chrome/common/extensions/api/file_system_provider_internal.h"
#include "components/services/filesystem/public/mojom/types.mojom.h"

namespace ash::file_system_provider::operations {
namespace {

// Convert |input| into |output|. If parsing fails, then returns false.
bool ConvertRequestValueToEntryList(const RequestValue& value,
                                    storage::AsyncFileUtil::EntryList* output) {
  using extensions::api::file_system_provider::EntryMetadata;
  using extensions::api::file_system_provider_internal::
      ReadDirectoryRequestedSuccess::Params;

  const Params* params = value.read_directory_success_params();
  if (!params)
    return false;

  for (const EntryMetadata& entry_metadata : params->entries) {
    if (!ValidateIDLEntryMetadata(
            entry_metadata,
            ProvidedFileSystemInterface::METADATA_FIELD_IS_DIRECTORY |
                ProvidedFileSystemInterface::METADATA_FIELD_NAME,
            /*root_entry=*/false)) {
      return false;
    }

    output->emplace_back(base::FilePath(*entry_metadata.name), base::FilePath(),
                         *entry_metadata.is_directory
                             ? filesystem::mojom::FsFileType::DIRECTORY
                             : filesystem::mojom::FsFileType::REGULAR_FILE);
  }

  return true;
}

}  // namespace

ReadDirectory::ReadDirectory(
    RequestDispatcher* dispatcher,
    const ProvidedFileSystemInfo& file_system_info,
    const base::FilePath& directory_path,
    storage::AsyncFileUtil::ReadDirectoryCallback callback)
    : Operation(dispatcher, file_system_info),
      directory_path_(directory_path),
      callback_(std::move(callback)) {}

ReadDirectory::~ReadDirectory() = default;

bool ReadDirectory::Execute(int request_id) {
  using extensions::api::file_system_provider::ReadDirectoryRequestedOptions;

  ReadDirectoryRequestedOptions options;
  options.file_system_id = file_system_info_.file_system_id();
  options.request_id = request_id;
  options.directory_path = directory_path_.AsUTF8Unsafe();

  // Request only is_directory and name metadata fields.
  options.is_directory = true;
  options.name = true;

  return SendEvent(
      request_id,
      extensions::events::FILE_SYSTEM_PROVIDER_ON_READ_DIRECTORY_REQUESTED,
      extensions::api::file_system_provider::OnReadDirectoryRequested::
          kEventName,
      extensions::api::file_system_provider::OnReadDirectoryRequested::Create(
          options));
}

void ReadDirectory::OnSuccess(/*request_id=*/int,
                              const RequestValue& result,
                              bool has_more) {
  storage::AsyncFileUtil::EntryList entry_list;
  const bool convert_result =
      ConvertRequestValueToEntryList(result, &entry_list);

  if (!convert_result) {
    LOG(ERROR)
        << "Failed to parse a response for the read directory operation.";
    callback_.Run(base::File::FILE_ERROR_IO,
                  storage::AsyncFileUtil::EntryList(),
                  /*has_more=*/false);
    return;
  }

  callback_.Run(base::File::FILE_OK, entry_list, has_more);
}

void ReadDirectory::OnError(/*request_id=*/int,
                            /*result=*/const RequestValue&,
                            base::File::Error error) {
  callback_.Run(error, storage::AsyncFileUtil::EntryList(), /*has_more=*/false);
}

}  // namespace ash::file_system_provider::operations
