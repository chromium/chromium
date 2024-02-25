// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/io_task_util.h"

#include <memory>

#include "content/public/browser/browser_thread.h"

namespace file_manager {
namespace io_task {

void GetFileMetadataOnIOThread(
    scoped_refptr<storage::FileSystemContext> file_system_context,
    const storage::FileSystemURL& url,
    storage::FileSystemOperation::GetMetadataFieldSet fields,
    storage::FileSystemOperation::GetMetadataCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  file_system_context->operation_runner()->GetMetadata(url, fields,
                                                       std::move(callback));
}

storage::FileSystemOperationRunner::OperationID StartDeleteOnIOThread(
    scoped_refptr<storage::FileSystemContext> file_system_context,
    const storage::FileSystemURL& file_url,
    storage::FileSystemOperation::StatusCallback status_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  return file_system_context->operation_runner()->Remove(
      file_url, /*recursive=*/true, std::move(status_callback));
}

storage::FileSystemOperationRunner::OperationID StartMoveFileLocalOnIOThread(
    scoped_refptr<storage::FileSystemContext> file_system_context,
    const storage::FileSystemURL source_url,
    const storage::FileSystemURL destination_url,
    storage::FileSystemOperation::CopyOrMoveOptionSet options,
    storage::FileSystemOperation::StatusCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  return file_system_context->operation_runner()->MoveFileLocal(
      source_url, destination_url, options, std::move(callback));
}

std::ostream& operator<<(std::ostream& out, const ProgressStatus& value) {
  out << "{ Status: " << static_cast<int>(value.state);
  out << " , Sources: ";
  if (value.sources.size() == 0) {
    out << "none";
  }
  for (const auto& entry_status : value.sources) {
    out << "[ " << entry_status.url.path() << " , ";
    if (entry_status.error.has_value()) {
      out << base::File::ErrorToString(entry_status.error.value());
    } else {
      out << "no error";
    }
    out << " ] ";
  }
  out << ", Outputs : ";
  if (value.outputs.size() == 0) {
    out << "none }";
    return out;
  }
  for (const auto& entry_status : value.outputs) {
    out << " [ " << entry_status.url.path() << " , ";
    if (entry_status.error.has_value()) {
      out << base::File::ErrorToString(entry_status.error.value());
    } else {
      out << "no error";
    }
    out << " ] ";
  }
  out << "}";
  return out;
}

}  // namespace io_task
}  // namespace file_manager
