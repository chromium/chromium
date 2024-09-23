// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/fileapi/arc_documents_provider_backend_delegate.h"

#include <utility>

#include "base/check_op.h"
#include "base/notreached.h"
#include "chrome/browser/ash/arc/fileapi/arc_documents_provider_file_stream_reader.h"
#include "chrome/browser/ash/arc/fileapi/arc_documents_provider_file_stream_writer.h"
#include "content/public/browser/browser_thread.h"
#include "storage/browser/file_system/file_stream_reader.h"
#include "storage/browser/file_system/file_stream_writer.h"
#include "url/gurl.h"

using content::BrowserThread;

namespace arc {

ArcDocumentsProviderBackendDelegate::ArcDocumentsProviderBackendDelegate() =
    default;

ArcDocumentsProviderBackendDelegate::~ArcDocumentsProviderBackendDelegate() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

storage::AsyncFileUtil* ArcDocumentsProviderBackendDelegate::GetAsyncFileUtil(
    storage::FileSystemType type) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return &async_file_util_;
}

std::unique_ptr<storage::FileStreamReader>
ArcDocumentsProviderBackendDelegate::CreateFileStreamReader(
    const storage::FileSystemURL& url,
    int64_t offset,
    int64_t max_bytes_to_read,
    const base::Time& expected_modification_time,
    storage::FileSystemContext* context) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  return std::make_unique<ArcDocumentsProviderFileStreamReader>(url, offset);
}

std::unique_ptr<storage::FileStreamWriter>
ArcDocumentsProviderBackendDelegate::CreateFileStreamWriter(
    const storage::FileSystemURL& url,
    int64_t offset,
    storage::FileSystemContext* context) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  return std::make_unique<ArcDocumentsProviderFileStreamWriter>(url, offset);
}

storage::WatcherManager* ArcDocumentsProviderBackendDelegate::GetWatcherManager(
    storage::FileSystemType type) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return &watcher_manager_;
}

}  // namespace arc
