// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ash/file_system_provider/operations/read_file.h"

#include <stddef.h>

#include <limits>
#include <string>
#include <utility>

#include "base/trace_event/trace_event.h"
#include "chrome/common/extensions/api/file_system_provider.h"
#include "chrome/common/extensions/api/file_system_provider_internal.h"

namespace ash::file_system_provider::operations {
namespace {

// Convert |value| into |output|. If parsing fails, then returns a negative
// value. Otherwise returns number of bytes written to the buffer.
int CopyRequestValueToBuffer(const RequestValue& value,
                             scoped_refptr<net::IOBuffer> buffer,
                             int buffer_offset,
                             int buffer_length) {
  using extensions::api::file_system_provider_internal::
      ReadFileRequestedSuccess::Params;

  const Params* params = value.read_file_success_params();
  if (!params)
    return -1;

  const size_t chunk_size = params->data.size();

  // Check for overflows.
  if (chunk_size > static_cast<size_t>(buffer_length) - buffer_offset)
    return -1;

  memcpy(buffer->data() + buffer_offset, params->data.data(), chunk_size);

  return chunk_size;
}

}  // namespace

ReadFile::ReadFile(
    RequestDispatcher* dispatcher,
    const ProvidedFileSystemInfo& file_system_info,
    int file_handle,
    scoped_refptr<net::IOBuffer> buffer,
    int64_t offset,
    int length,
    ProvidedFileSystemInterface::ReadChunkReceivedCallback callback)
    : Operation(dispatcher, file_system_info),
      file_handle_(file_handle),
      buffer_(buffer),
      offset_(offset),
      length_(length),
      current_offset_(0),
      callback_(std::move(callback)) {}

ReadFile::~ReadFile() = default;

bool ReadFile::Execute(int request_id) {
  using extensions::api::file_system_provider::ReadFileRequestedOptions;
  TRACE_EVENT0("file_system_provider", "ReadFile::Execute");

  ReadFileRequestedOptions options;
  options.file_system_id = file_system_info_.file_system_id();
  options.request_id = request_id;
  options.open_request_id = file_handle_;
  options.offset = offset_;
  options.length = length_;

  return SendEvent(
      request_id,
      extensions::events::FILE_SYSTEM_PROVIDER_ON_READ_FILE_REQUESTED,
      extensions::api::file_system_provider::OnReadFileRequested::kEventName,
      extensions::api::file_system_provider::OnReadFileRequested::Create(
          options));
}

void ReadFile::OnSuccess(/*request_id=*/int,
                         const RequestValue& result,
                         bool has_more) {
  TRACE_EVENT0("file_system_provider", "ReadFile::OnSuccess");
  const int copy_result =
      CopyRequestValueToBuffer(result, buffer_, current_offset_, length_);

  if (copy_result < 0) {
    LOG(ERROR) << "Failed to parse a response for the read file operation.";
    callback_.Run(
        /*chunk_length=*/0, /*has_more=*/false, base::File::FILE_ERROR_IO);
    return;
  }

  if (copy_result > 0)
    current_offset_ += copy_result;
  callback_.Run(copy_result, has_more, base::File::FILE_OK);
}

void ReadFile::OnError(/*request_id=*/int,
                       /*result=*/const RequestValue&,
                       base::File::Error error) {
  TRACE_EVENT0("file_system_provider", "ReadFile::OnError");
  callback_.Run(/*chunk_length=*/0, /*has_more=*/false, error);
}

}  // namespace ash::file_system_provider::operations
