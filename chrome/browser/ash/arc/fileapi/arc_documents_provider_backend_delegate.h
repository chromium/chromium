// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_FILEAPI_ARC_DOCUMENTS_PROVIDER_BACKEND_DELEGATE_H_
#define CHROME_BROWSER_ASH_ARC_FILEAPI_ARC_DOCUMENTS_PROVIDER_BACKEND_DELEGATE_H_

#include <memory>

#include "chrome/browser/ash/arc/fileapi/arc_documents_provider_async_file_util.h"
#include "chrome/browser/ash/arc/fileapi/arc_documents_provider_root_map.h"
#include "chrome/browser/ash/arc/fileapi/arc_documents_provider_watcher_manager.h"
#include "chrome/browser/ash/fileapi/file_system_backend_delegate.h"

namespace arc {

// Implements ARC documents provider filesystem.
class ArcDocumentsProviderBackendDelegate
    : public ash::FileSystemBackendDelegate {
 public:
  ArcDocumentsProviderBackendDelegate();

  ArcDocumentsProviderBackendDelegate(
      const ArcDocumentsProviderBackendDelegate&) = delete;
  ArcDocumentsProviderBackendDelegate& operator=(
      const ArcDocumentsProviderBackendDelegate&) = delete;

  ~ArcDocumentsProviderBackendDelegate() override;

  // FileSystemBackend::Delegate overrides.
  storage::AsyncFileUtil* GetAsyncFileUtil(
      storage::FileSystemType type) override;
  std::unique_ptr<storage::FileStreamReader> CreateFileStreamReader(
      const storage::FileSystemURL& url,
      int64_t offset,
      int64_t max_bytes_to_read,
      const base::Time& expected_modification_time,
      storage::FileSystemContext* context) override;
  std::unique_ptr<storage::FileStreamWriter> CreateFileStreamWriter(
      const storage::FileSystemURL& url,
      int64_t offset,
      storage::FileSystemContext* context) override;
  storage::WatcherManager* GetWatcherManager(
      storage::FileSystemType type) override;

 private:
  ArcDocumentsProviderAsyncFileUtil async_file_util_;
  ArcDocumentsProviderWatcherManager watcher_manager_;
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_FILEAPI_ARC_DOCUMENTS_PROVIDER_BACKEND_DELEGATE_H_
