// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/fileapi/file_system_backend_delegate.h"

#include <memory>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/task/post_task.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/drive/fileapi/async_file_util.h"
#include "chrome/browser/chromeos/drive/fileapi/fileapi_worker.h"
#include "chrome/browser/chromeos/drive/fileapi/webkit_file_stream_reader_impl.h"
#include "chrome/browser/chromeos/drive/fileapi/webkit_file_stream_writer_impl.h"
#include "components/drive/chromeos/file_system_interface.h"
#include "components/drive/drive_api_util.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "storage/browser/fileapi/async_file_util.h"
#include "storage/browser/fileapi/file_stream_reader.h"
#include "storage/browser/fileapi/file_system_context.h"
#include "storage/browser/fileapi/file_system_url.h"

using content::BrowserThread;

namespace drive {
namespace {

// Called on the UI thread after GetRedirectURLForContentsOnUIThread. Obtains
// the browser URL from |entry|. |callback| will be called on the IO thread.
void GetRedirectURLForContentsOnUIThreadWithResourceEntry(
    const storage::URLCallback& callback,
    FileError error,
    std::unique_ptr<ResourceEntry> entry) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  GURL url;
  if (error == FILE_ERROR_OK && entry->has_file_specific_info() &&
      entry->file_specific_info().is_hosted_document()) {
    url = GURL(entry->alternate_url());
  }
  base::PostTaskWithTraits(FROM_HERE, {BrowserThread::IO},
                           base::BindOnce(callback, url));
}

// Called on the UI thread after
// FileSystemBackendDelegate::GetRedirectURLForContents.  Requestes to obtain
// ResourceEntry for the |url|.
void GetRedirectURLForContentsOnUIThread(
    const storage::FileSystemURL& url,
    const storage::URLCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  FileSystemInterface* const file_system =
      fileapi_internal::GetFileSystemFromUrl(url);
  if (!file_system) {
    base::PostTaskWithTraits(FROM_HERE, {BrowserThread::IO},
                             base::BindOnce(callback, GURL()));
    return;
  }
  const base::FilePath file_path = util::ExtractDrivePathFromFileSystemUrl(url);
  if (file_path.empty()) {
    base::PostTaskWithTraits(FROM_HERE, {BrowserThread::IO},
                             base::BindOnce(callback, GURL()));
    return;
  }

  file_system->GetResourceEntry(
      file_path,
      base::BindOnce(&GetRedirectURLForContentsOnUIThreadWithResourceEntry,
                     callback));
}

}  // namespace

FileSystemBackendDelegate::FileSystemBackendDelegate()
    : async_file_util_(new internal::AsyncFileUtil) {
}

FileSystemBackendDelegate::~FileSystemBackendDelegate() = default;

storage::AsyncFileUtil* FileSystemBackendDelegate::GetAsyncFileUtil(
    storage::FileSystemType type) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_EQ(storage::kFileSystemTypeDrive, type);
  return async_file_util_.get();
}

std::unique_ptr<storage::FileStreamReader>
FileSystemBackendDelegate::CreateFileStreamReader(
    const storage::FileSystemURL& url,
    int64_t offset,
    int64_t max_bytes_to_read,
    const base::Time& expected_modification_time,
    storage::FileSystemContext* context) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_EQ(storage::kFileSystemTypeDrive, url.type());

  base::FilePath file_path = util::ExtractDrivePathFromFileSystemUrl(url);
  if (file_path.empty())
    return std::unique_ptr<storage::FileStreamReader>();

  return std::unique_ptr<storage::FileStreamReader>(
      new internal::WebkitFileStreamReaderImpl(
          base::Bind(&fileapi_internal::GetFileSystemFromUrl, url),
          context->default_file_task_runner(), file_path, offset,
          expected_modification_time));
}

std::unique_ptr<storage::FileStreamWriter>
FileSystemBackendDelegate::CreateFileStreamWriter(
    const storage::FileSystemURL& url,
    int64_t offset,
    storage::FileSystemContext* context) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_EQ(storage::kFileSystemTypeDrive, url.type());

  base::FilePath file_path = util::ExtractDrivePathFromFileSystemUrl(url);
  // Hosted documents don't support stream writer.
  if (file_path.empty() || util::HasHostedDocumentExtension(file_path))
    return std::unique_ptr<storage::FileStreamWriter>();

  return std::unique_ptr<storage::FileStreamWriter>(
      new internal::WebkitFileStreamWriterImpl(
          base::Bind(&fileapi_internal::GetFileSystemFromUrl, url),
          context->default_file_task_runner(), file_path, offset));
}

storage::WatcherManager* FileSystemBackendDelegate::GetWatcherManager(
    storage::FileSystemType type) {
  NOTIMPLEMENTED();
  return nullptr;
}

void FileSystemBackendDelegate::GetRedirectURLForContents(
    const storage::FileSystemURL& url,
    const storage::URLCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&GetRedirectURLForContentsOnUIThread, url, callback));
}

}  // namespace drive
