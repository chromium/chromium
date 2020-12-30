// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILEAPI_OBSERVABLE_FILE_SYSTEM_OPERATION_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_FILEAPI_OBSERVABLE_FILE_SYSTEM_OPERATION_IMPL_H_

#include <memory>

#include "components/account_id/account_id.h"
#include "storage/browser/file_system/file_system_operation_impl.h"

namespace chromeos {

// A thin extension to the default `storage::FileSystemOperationImpl` which
// notifies the `FileChangeService` of file change events.
class ObservableFileSystemOperationImpl
    : public storage::FileSystemOperationImpl {
 public:
  ObservableFileSystemOperationImpl(
      const AccountId& account_id,
      const storage::FileSystemURL& url,
      storage::FileSystemContext* file_system_context,
      std::unique_ptr<storage::FileSystemOperationContext> operation_context);
  ObservableFileSystemOperationImpl(const ObservableFileSystemOperationImpl&) =
      delete;
  ObservableFileSystemOperationImpl& operator=(
      const ObservableFileSystemOperationImpl&) = delete;
  ~ObservableFileSystemOperationImpl() override;

 private:
  // storage::FileSystemOperationImpl:
  void Copy(const storage::FileSystemURL& src,
            const storage::FileSystemURL& dst,
            CopyOrMoveOption option,
            ErrorBehavior error_behavior,
            const CopyProgressCallback& progress_callback,
            StatusCallback callback) override;
  void CopyFileLocal(const storage::FileSystemURL& src,
                     const storage::FileSystemURL& dst,
                     CopyOrMoveOption option,
                     const CopyFileProgressCallback& progress_callback,
                     StatusCallback callback) override;
  void Move(const storage::FileSystemURL& src,
            const storage::FileSystemURL& dst,
            CopyOrMoveOption option,
            StatusCallback callback) override;
  void MoveFileLocal(const storage::FileSystemURL& src,
                     const storage::FileSystemURL& dst,
                     CopyOrMoveOption option,
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

  const AccountId account_id_;
  base::WeakPtrFactory<ObservableFileSystemOperationImpl> weak_factory_{this};
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FILEAPI_OBSERVABLE_FILE_SYSTEM_OPERATION_IMPL_H_
