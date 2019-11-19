// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_LOCAL_SYNCABLE_FILE_SYSTEM_OPERATION_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_LOCAL_SYNCABLE_FILE_SYSTEM_OPERATION_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "storage/browser/file_system/file_system_operation.h"
#include "storage/browser/file_system/file_system_url.h"

namespace storage {
class FileSystemContext;
class FileSystemOperationContext;
}

namespace sync_file_system {

class SyncableFileOperationRunner;

// A wrapper class of FileSystemOperation for syncable file system.
class SyncableFileSystemOperation : public storage::FileSystemOperation {
 public:
  ~SyncableFileSystemOperation() override;

  // storage::FileSystemOperation overrides.
  void CreateFile(const storage::FileSystemURL& url,
                  bool exclusive,
                  StatusCallback callback) override;
  void CreateDirectory(const storage::FileSystemURL& url,
                       bool exclusive,
                       bool recursive,
                       StatusCallback callback) override;
  void Copy(const storage::FileSystemURL& src_url,
            const storage::FileSystemURL& dest_url,
            CopyOrMoveOption option,
            ErrorBehavior error_behavior,
            const CopyProgressCallback& progress_callback,
            StatusCallback callback) override;
  void Move(const storage::FileSystemURL& src_url,
            const storage::FileSystemURL& dest_url,
            CopyOrMoveOption option,
            StatusCallback callback) override;
  void DirectoryExists(const storage::FileSystemURL& url,
                       StatusCallback callback) override;
  void FileExists(const storage::FileSystemURL& url,
                  StatusCallback callback) override;
  void GetMetadata(const storage::FileSystemURL& url,
                   int fields,
                   GetMetadataCallback callback) override;
  void ReadDirectory(const storage::FileSystemURL& url,
                     const ReadDirectoryCallback& callback) override;
  void Remove(const storage::FileSystemURL& url,
              bool recursive,
              StatusCallback callback) override;
  void WriteBlob(const storage::FileSystemURL& url,
                 std::unique_ptr<storage::FileWriterDelegate> writer_delegate,
                 std::unique_ptr<storage::BlobReader> blob_reader,
                 const WriteCallback& callback) override;
  void Write(const storage::FileSystemURL& url,
             std::unique_ptr<storage::FileWriterDelegate> writer_delegate,
             mojo::ScopedDataPipeConsumerHandle data_pipe,
             const WriteCallback& callback) override;
  void Truncate(const storage::FileSystemURL& url,
                int64_t length,
                StatusCallback callback) override;
  void TouchFile(const storage::FileSystemURL& url,
                 const base::Time& last_access_time,
                 const base::Time& last_modified_time,
                 StatusCallback callback) override;
  void OpenFile(const storage::FileSystemURL& url,
                int file_flags,
                OpenFileCallback callback) override;
  void Cancel(StatusCallback cancel_callback) override;
  void CreateSnapshotFile(const storage::FileSystemURL& path,
                          SnapshotFileCallback callback) override;
  void CopyInForeignFile(const base::FilePath& src_local_disk_path,
                         const storage::FileSystemURL& dest_url,
                         StatusCallback callback) override;
  void RemoveFile(const storage::FileSystemURL& url,
                  StatusCallback callback) override;
  void RemoveDirectory(const storage::FileSystemURL& url,
                       StatusCallback callback) override;
  void CopyFileLocal(const storage::FileSystemURL& src_url,
                     const storage::FileSystemURL& dest_url,
                     CopyOrMoveOption option,
                     const CopyFileProgressCallback& progress_callback,
                     StatusCallback callback) override;
  void MoveFileLocal(const storage::FileSystemURL& src_url,
                     const storage::FileSystemURL& dest_url,
                     CopyOrMoveOption option,
                     StatusCallback callback) override;
  base::File::Error SyncGetPlatformPath(const storage::FileSystemURL& url,
                                        base::FilePath* platform_path) override;

 private:
  typedef SyncableFileSystemOperation self;
  class QueueableTask;

  // Only SyncFileSystemBackend can create a new operation directly.
  friend class SyncFileSystemBackend;

  SyncableFileSystemOperation(
      const storage::FileSystemURL& url,
      storage::FileSystemContext* file_system_context,
      std::unique_ptr<storage::FileSystemOperationContext> operation_context);

  void DidFinish(base::File::Error status);
  void DidWrite(const WriteCallback& callback,
                base::File::Error result,
                int64_t bytes,
                bool complete);

  void OnCancelled();

  const storage::FileSystemURL url_;

  std::unique_ptr<storage::FileSystemOperation> impl_;
  base::WeakPtr<SyncableFileOperationRunner> operation_runner_;
  std::vector<storage::FileSystemURL> target_paths_;

  StatusCallback completion_callback_;

  base::WeakPtrFactory<SyncableFileSystemOperation> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SyncableFileSystemOperation);
};

}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_LOCAL_SYNCABLE_FILE_SYSTEM_OPERATION_H_
