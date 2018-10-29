// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_system_provider/operations/get_metadata.h"

#include <stdint.h>

#include <algorithm>
#include <string>
#include <utility>

#include "base/macros.h"
#include "chrome/common/extensions/api/file_system_provider.h"
#include "chrome/common/extensions/api/file_system_provider_internal.h"

namespace chromeos {
namespace file_system_provider {
namespace operations {
namespace {

// Convert |value| into |output|. If parsing fails, then returns false.
bool ConvertRequestValueToFileInfo(std::unique_ptr<RequestValue> value,
                                   int fields,
                                   bool root_entry,
                                   EntryMetadata* output) {
  using extensions::api::file_system_provider::EntryMetadata;
  using extensions::api::file_system_provider_internal::
      GetMetadataRequestedSuccess::Params;

  const Params* params = value->get_metadata_success_params();
  if (!params)
    return false;

  if (!ValidateIDLEntryMetadata(params->metadata, fields, root_entry))
    return false;

  if (fields & ProvidedFileSystemInterface::METADATA_FIELD_NAME)
    output->name.reset(new std::string(*params->metadata.name));

  if (fields & ProvidedFileSystemInterface::METADATA_FIELD_IS_DIRECTORY)
    output->is_directory.reset(new bool(*params->metadata.is_directory));

  if (fields & ProvidedFileSystemInterface::METADATA_FIELD_SIZE)
    output->size.reset(
        new int64_t(static_cast<int64_t>(*params->metadata.size)));

  if (fields & ProvidedFileSystemInterface::METADATA_FIELD_MODIFICATION_TIME) {
    std::string input_modification_time;
    params->metadata.modification_time->additional_properties.GetString(
        "value", &input_modification_time);

    // Allow to pass invalid modification time, since there is no way to verify
    // it easily on any earlier stage.
    base::Time output_modification_time;
    ignore_result(base::Time::FromString(input_modification_time.c_str(),
                                         &output_modification_time));
    output->modification_time.reset(new base::Time(output_modification_time));
  }

  if (fields & ProvidedFileSystemInterface::METADATA_FIELD_MIME_TYPE &&
      params->metadata.mime_type.get()) {
    output->mime_type.reset(new std::string(*params->metadata.mime_type.get()));
  }

  if (fields & ProvidedFileSystemInterface::METADATA_FIELD_THUMBNAIL &&
      params->metadata.thumbnail.get()) {
    output->thumbnail.reset(new std::string(*params->metadata.thumbnail.get()));
  }

  return true;
}

}  // namespace

bool ValidateIDLEntryMetadata(
    const extensions::api::file_system_provider::EntryMetadata& metadata,
    int fields,
    bool root_entry) {
  using extensions::api::file_system_provider::EntryMetadata;

  if (fields & ProvidedFileSystemInterface::METADATA_FIELD_IS_DIRECTORY &&
      !metadata.is_directory.get()) {
    return false;
  }

  if (fields & ProvidedFileSystemInterface::METADATA_FIELD_NAME &&
      (!metadata.name.get() || !ValidateName(*metadata.name, root_entry))) {
    return false;
  }

  if (fields & ProvidedFileSystemInterface::METADATA_FIELD_SIZE &&
      !metadata.size.get()) {
    return false;
  }

  if (fields & ProvidedFileSystemInterface::METADATA_FIELD_MODIFICATION_TIME) {
    if (!metadata.modification_time)
      return false;
    std::string input_modification_time;
    if (!metadata.modification_time->additional_properties.GetString(
            "value", &input_modification_time)) {
      return false;
    }
  }

  // Empty MIME type is not allowed, but for backward compability it's
  // accepted. Note, that there is a warning in custom bindings for it.

  if (fields & ProvidedFileSystemInterface::METADATA_FIELD_THUMBNAIL &&
      metadata.thumbnail.get()) {
    // Sanity check for the thumbnail format. Note, that another, more
    // granural check is done in custom bindings. Note, this is an extra check
    // only for the security reasons.
    const std::string expected_prefix = "data:";
    std::string thumbnail_prefix =
        metadata.thumbnail.get()->substr(0, expected_prefix.size());
    std::transform(thumbnail_prefix.begin(),
                   thumbnail_prefix.end(),
                   thumbnail_prefix.begin(),
                   ::tolower);

    if (expected_prefix != thumbnail_prefix)
      return false;
  }

  return true;
}

bool ValidateName(const std::string& name, bool root_entry) {
  if (root_entry)
    return name.empty();
  return !name.empty() && name.find('/') == std::string::npos;
}

GetMetadata::GetMetadata(
    extensions::EventRouter* event_router,
    const ProvidedFileSystemInfo& file_system_info,
    const base::FilePath& entry_path,
    ProvidedFileSystemInterface::MetadataFieldMask fields,
    ProvidedFileSystemInterface::GetMetadataCallback callback)
    : Operation(event_router, file_system_info),
      entry_path_(entry_path),
      fields_(fields),
      callback_(std::move(callback)) {
  DCHECK_NE(0, fields_);
}

GetMetadata::~GetMetadata() {
}

bool GetMetadata::Execute(int request_id) {
  using extensions::api::file_system_provider::GetMetadataRequestedOptions;

  GetMetadataRequestedOptions options;
  options.file_system_id = file_system_info_.file_system_id();
  options.request_id = request_id;
  options.entry_path = entry_path_.AsUTF8Unsafe();
  options.is_directory =
      fields_ & ProvidedFileSystemInterface::METADATA_FIELD_IS_DIRECTORY;
  options.name = fields_ & ProvidedFileSystemInterface::METADATA_FIELD_NAME;
  options.size = fields_ & ProvidedFileSystemInterface::METADATA_FIELD_SIZE;
  options.modification_time =
      fields_ & ProvidedFileSystemInterface::METADATA_FIELD_MODIFICATION_TIME;
  options.mime_type =
      fields_ & ProvidedFileSystemInterface::METADATA_FIELD_MIME_TYPE;
  options.thumbnail =
      fields_ & ProvidedFileSystemInterface::METADATA_FIELD_THUMBNAIL;

  return SendEvent(
      request_id,
      extensions::events::FILE_SYSTEM_PROVIDER_ON_GET_METADATA_REQUESTED,
      extensions::api::file_system_provider::OnGetMetadataRequested::kEventName,
      extensions::api::file_system_provider::OnGetMetadataRequested::Create(
          options));
}

void GetMetadata::OnSuccess(int /* request_id */,
                            std::unique_ptr<RequestValue> result,
                            bool has_more) {
  DCHECK(callback_);
  std::unique_ptr<EntryMetadata> metadata(new EntryMetadata);
  const bool convert_result = ConvertRequestValueToFileInfo(
      std::move(result), fields_,
      entry_path_.AsUTF8Unsafe() == FILE_PATH_LITERAL("/"), metadata.get());

  if (!convert_result) {
    LOG(ERROR) << "Failed to parse a response for the get metadata operation.";
    std::move(callback_).Run(nullptr, base::File::FILE_ERROR_IO);
    return;
  }

  std::move(callback_).Run(std::move(metadata), base::File::FILE_OK);
}

void GetMetadata::OnError(int /* request_id */,
                          std::unique_ptr<RequestValue> /* result */,
                          base::File::Error error) {
  DCHECK(callback_);
  std::move(callback_).Run(nullptr, error);
}

}  // namespace operations
}  // namespace file_system_provider
}  // namespace chromeos
