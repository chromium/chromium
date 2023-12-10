// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_DEVICE_MEDIA_ASYNC_FILE_UTIL_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_DEVICE_MEDIA_ASYNC_FILE_UTIL_H_

#include <stdint.h>

#include <memory>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "storage/browser/blob/shareable_file_reference.h"
#include "storage/browser/file_system/async_file_util.h"
#include "storage/browser/file_system/watcher_manager.h"

namespace storage {
class FileSystemOperationContext;
class FileSystemURL;
}

namespace storage {
class FileStreamReader;
}

enum MediaFileValidationType {
  NO_MEDIA_FILE_VALIDATION,
  APPLY_MEDIA_FILE_VALIDATION,
};

class DeviceMediaAsyncFileUtil : public storage::AsyncFileUtil {
 public:
  DeviceMediaAsyncFileUtil(const DeviceMediaAsyncFileUtil&) = delete;
  DeviceMediaAsyncFileUtil& operator=(const DeviceMediaAsyncFileUtil&) = delete;

  ~DeviceMediaAsyncFileUtil() override;

  // Returns an instance of DeviceMediaAsyncFileUtil.
  static std::unique_ptr<DeviceMediaAsyncFileUtil> Create(
      const base::FilePath& profile_path,
      MediaFileValidationType validation_type);

  bool SupportsStreaming(const storage::FileSystemURL& url);

  // AsyncFileUtil overrides.
  void CreateOrOpen(
      std::unique_ptr<storage::FileSystemOperationContext> context,
      const storage::FileSystemURL& url,
      uint32_t file_flags,
      CreateOrOpenCallback callback) override;
  void EnsureFileExists(
      std::unique_ptr<storage::FileSystemOperationContext> context,
      const storage::FileSystemURL& url,
      EnsureFileExistsCallback callback) override;
  void CreateDirectory(
      std::unique_ptr<storage::FileSystemOperationContext> context,
      const storage::FileSystemURL& url,
      bool exclusive,
      bool recursive,
      StatusCallback callback) override;
  void GetFileInfo(std::unique_ptr<storage::FileSystemOperationContext> context,
                   const storage::FileSystemURL& url,
                   GetMetadataFieldSet fields,
                   GetFileInfoCallback callback) override;
  void ReadDirectory(
      std::unique_ptr<storage::FileSystemOperationContext> context,
      const storage::FileSystemURL& url,
      ReadDirectoryCallback callback) override;
  void Touch(std::unique_ptr<storage::FileSystemOperationContext> context,
             const storage::FileSystemURL& url,
             const base::Time& last_access_time,
             const base::Time& last_modified_time,
             StatusCallback callback) override;
  void Truncate(std::unique_ptr<storage::FileSystemOperationContext> context,
                const storage::FileSystemURL& url,
                int64_t length,
                StatusCallback callback) override;
  void CopyFileLocal(
      std::unique_ptr<storage::FileSystemOperationContext> context,
      const storage::FileSystemURL& src_url,
      const storage::FileSystemURL& dest_url,
      CopyOrMoveOptionSet options,
      CopyFileProgressCallback progress_callback,
      StatusCallback callback) override;
  void MoveFileLocal(
      std::unique_ptr<storage::FileSystemOperationContext> context,
      const storage::FileSystemURL& src_url,
      const storage::FileSystemURL& dest_url,
      CopyOrMoveOptionSet options,
      StatusCallback callback) override;
  void CopyInForeignFile(
      std::unique_ptr<storage::FileSystemOperationContext> context,
      const base::FilePath& src_file_path,
      const storage::FileSystemURL& dest_url,
      StatusCallback callback) override;
  void DeleteFile(std::unique_ptr<storage::FileSystemOperationContext> context,
                  const storage::FileSystemURL& url,
                  StatusCallback callback) override;
  void DeleteDirectory(
      std::unique_ptr<storage::FileSystemOperationContext> context,
      const storage::FileSystemURL& url,
      StatusCallback callback) override;
  void DeleteRecursively(
      std::unique_ptr<storage::FileSystemOperationContext> context,
      const storage::FileSystemURL& url,
      StatusCallback callback) override;
  void CreateSnapshotFile(
      std::unique_ptr<storage::FileSystemOperationContext> context,
      const storage::FileSystemURL& url,
      CreateSnapshotFileCallback callback) override;

  // This method is called when existing Blobs are read.
  // |expected_modification_time| indicates the expected snapshot state of the
  // underlying storage. The returned FileStreamReader must return an error
  // when the state of the underlying storage changes. Any errors associated
  // with reading this file are returned by the FileStreamReader itself.
  virtual std::unique_ptr<storage::FileStreamReader> GetFileStreamReader(
      const storage::FileSystemURL& url,
      int64_t offset,
      const base::Time& expected_modification_time,
      storage::FileSystemContext* context);

  // Adds watcher to |url|.
  void AddWatcher(
      const storage::FileSystemURL& url,
      bool recursive,
      storage::WatcherManager::StatusCallback callback,
      storage::WatcherManager::NotificationCallback notification_callback);

  // Removes watcher of |url|.
  void RemoveWatcher(const storage::FileSystemURL& url,
                     const bool recursive,
                     storage::WatcherManager::StatusCallback callback);

 private:
  class MediaPathFilterWrapper;

  // Use Create() to get an instance of DeviceMediaAsyncFileUtil.
  DeviceMediaAsyncFileUtil(const base::FilePath& profile_path,
                           MediaFileValidationType validation_type);

  // Called when CreateDirectory method call succeeds. |callback| is invoked to
  // complete the CreateDirectory request.
  void OnDidCreateDirectory(StatusCallback callback);

  // Called when GetFileInfo method call succeeds. |file_info| contains the
  // file details of the requested url. |callback| is invoked to complete the
  // GetFileInfo request.
  void OnDidGetFileInfo(base::SequencedTaskRunner* task_runner,
                        const base::FilePath& path,
                        GetFileInfoCallback callback,
                        const base::File::Info& file_info);

  // Called when ReadDirectory method call succeeds. |callback| is invoked to
  // complete the ReadDirectory request.
  //
  // If the contents of the given directory are reported in one batch, then
  // |file_list| will have the list of all files/directories in the given
  // directory and |has_more| will be false.
  //
  // If the contents of the given directory are reported in multiple chunks,
  // |file_list| will have only a subset of all contents (the subsets reported
  // in any two calls are disjoint), and |has_more| will be true, except for
  // the last chunk.
  void OnDidReadDirectory(base::SequencedTaskRunner* task_runner,
                          ReadDirectoryCallback callback,
                          EntryList file_list,
                          bool has_more);

  // Called when MoveFileLocal method call succeeds. |callback| is invoked to
  // complete the MoveFileLocal request.
  void OnDidMoveFileLocal(StatusCallback callback);

  // Called when CopyFileLocal method call succeeds. |callback| is invoked to
  // complete the CopyFileLocal request.
  void OnDidCopyFileLocal(StatusCallback callback);

  // Called when CopyInForeignFile method call succeeds. |callback| is invoked
  // to complete the CopyInForeignFile request.
  void OnDidCopyInForeignFile(StatusCallback callback);

  // Called when DeleteFile method call succeeeds. |callback| is invoked to
  // complete the DeleteFile request.
  void OnDidDeleteFile(StatusCallback callback);

  // Called when DeleteDirectory method call succeeds. |callback| is invoked to
  // complete the DeleteDirectory request.
  void OnDidDeleteDirectory(StatusCallback callback);

  bool validate_media_files() const;

  // Profile path.
  const base::FilePath profile_path_;

  scoped_refptr<MediaPathFilterWrapper> media_path_filter_wrapper_;

  // For callbacks that may run after destruction.
  base::WeakPtrFactory<DeviceMediaAsyncFileUtil> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_DEVICE_MEDIA_ASYNC_FILE_UTIL_H_
