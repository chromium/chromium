// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/fileapi/arc_content_file_system_backend_delegate.h"

#include "chrome/browser/ash/arc/fileapi/arc_content_file_system_async_file_util.h"
#include "chrome/browser/ash/arc/fileapi/arc_content_file_system_file_stream_reader.h"
#include "chrome/browser/ash/arc/fileapi/arc_content_file_system_url_util.h"
#include "content/public/browser/browser_thread.h"
#include "storage/browser/file_system/file_stream_writer.h"
#include "storage/browser/file_system/file_system_url.h"

namespace arc {

ArcContentFileSystemBackendDelegate::ArcContentFileSystemBackendDelegate()
    : async_file_util_(new ArcContentFileSystemAsyncFileUtil()) {}

ArcContentFileSystemBackendDelegate::~ArcContentFileSystemBackendDelegate() =
    default;

storage::AsyncFileUtil* ArcContentFileSystemBackendDelegate::GetAsyncFileUtil(
    storage::FileSystemType type) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK_EQ(storage::kFileSystemTypeArcContent, type);
  return async_file_util_.get();
}

std::unique_ptr<storage::FileStreamReader>
ArcContentFileSystemBackendDelegate::CreateFileStreamReader(
    const storage::FileSystemURL& url,
    int64_t offset,
    int64_t max_bytes_to_read,
    const base::Time& expected_modification_time,
    storage::FileSystemContext* context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK_EQ(storage::kFileSystemTypeArcContent, url.type());
  GURL arc_url = FileSystemUrlToArcUrl(url);
  return std::make_unique<ArcContentFileSystemFileStreamReader>(arc_url,
                                                                offset);
}

std::unique_ptr<storage::FileStreamWriter>
ArcContentFileSystemBackendDelegate::CreateFileStreamWriter(
    const storage::FileSystemURL& url,
    int64_t offset,
    storage::FileSystemContext* context) {
  NOTIMPLEMENTED();
  return nullptr;
}

storage::WatcherManager* ArcContentFileSystemBackendDelegate::GetWatcherManager(
    storage::FileSystemType type) {
  NOTIMPLEMENTED();
  return nullptr;
}

}  // namespace arc
