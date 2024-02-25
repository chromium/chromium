// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/fileapi/arc_content_file_system_async_file_util.h"

#include "ash/components/arc/arc_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/ash/arc/fileapi/arc_content_file_system_size_util.h"
#include "chrome/browser/ash/arc/fileapi/arc_content_file_system_url_util.h"
#include "chrome/browser/ash/arc/fileapi/arc_file_system_operation_runner_util.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/net_errors.h"
#include "storage/browser/blob/shareable_file_reference.h"

namespace arc {

namespace {

void OnGetFileSize(storage::AsyncFileUtil::GetFileInfoCallback callback,
                   int64_t size) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  base::File::Info info;
  base::File::Error error = base::File::FILE_OK;
  if (size == -1) {
    error = base::File::FILE_ERROR_FAILED;
  } else {
    info.size = size;
  }
  std::move(callback).Run(error, info);
}

}  // namespace

ArcContentFileSystemAsyncFileUtil::ArcContentFileSystemAsyncFileUtil() =
    default;

ArcContentFileSystemAsyncFileUtil::~ArcContentFileSystemAsyncFileUtil() =
    default;

void ArcContentFileSystemAsyncFileUtil::CreateOrOpen(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    uint32_t file_flags,
    CreateOrOpenCallback callback) {
  NOTIMPLEMENTED();
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), base::File(), base::OnceClosure()));
}

void ArcContentFileSystemAsyncFileUtil::EnsureFileExists(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    EnsureFileExistsCallback callback) {
  NOTIMPLEMENTED();
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback),
                                base::File::FILE_ERROR_FAILED, false));
}

void ArcContentFileSystemAsyncFileUtil::CreateDirectory(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    bool exclusive,
    bool recursive,
    StatusCallback callback) {
  NOTIMPLEMENTED();
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), base::File::FILE_ERROR_FAILED));
}

void ArcContentFileSystemAsyncFileUtil::GetFileInfo(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    GetMetadataFieldSet fields,
    GetFileInfoCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  file_system_operation_runner_util::GetFileSizeOnIOThread(
      FileSystemUrlToArcUrl(url),
      base::BindOnce(&OnGetFileSize, std::move(callback)));
}

void ArcContentFileSystemAsyncFileUtil::ReadDirectory(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    ReadDirectoryCallback callback) {
  NOTIMPLEMENTED();
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), base::File::FILE_ERROR_FAILED,
                     EntryList(), false));
}

void ArcContentFileSystemAsyncFileUtil::Touch(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    const base::Time& last_access_time,
    const base::Time& last_modified_time,
    StatusCallback callback) {
  NOTIMPLEMENTED();
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), base::File::FILE_ERROR_FAILED));
}

void ArcContentFileSystemAsyncFileUtil::Truncate(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    int64_t length,
    StatusCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  // Truncate() doesn't work well on ARC P/R container. It works on ARCVM R+
  // because the mojo proxy for ARCVM implements the feature.
  // TODO(b/223247850) Fix this.
  if (!IsArcVmEnabled()) {
    NOTIMPLEMENTED();
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), base::File::FILE_ERROR_FAILED));
    return;
  }
  TruncateOnIOThread(FileSystemUrlToArcUrl(url), length, std::move(callback));
}

void ArcContentFileSystemAsyncFileUtil::CopyFileLocal(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& src_url,
    const storage::FileSystemURL& dest_url,
    CopyOrMoveOptionSet options,
    CopyFileProgressCallback progress_callback,
    StatusCallback callback) {
  NOTIMPLEMENTED();
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), base::File::FILE_ERROR_FAILED));
}

void ArcContentFileSystemAsyncFileUtil::MoveFileLocal(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& src_url,
    const storage::FileSystemURL& dest_url,
    CopyOrMoveOptionSet options,
    StatusCallback callback) {
  NOTIMPLEMENTED();
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), base::File::FILE_ERROR_FAILED));
}

void ArcContentFileSystemAsyncFileUtil::CopyInForeignFile(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const base::FilePath& src_file_path,
    const storage::FileSystemURL& dest_url,
    StatusCallback callback) {
  NOTIMPLEMENTED();
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), base::File::FILE_ERROR_FAILED));
}

void ArcContentFileSystemAsyncFileUtil::DeleteFile(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    StatusCallback callback) {
  NOTIMPLEMENTED();
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), base::File::FILE_ERROR_FAILED));
}

void ArcContentFileSystemAsyncFileUtil::DeleteDirectory(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    StatusCallback callback) {
  NOTIMPLEMENTED();
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), base::File::FILE_ERROR_FAILED));
}

void ArcContentFileSystemAsyncFileUtil::DeleteRecursively(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    StatusCallback callback) {
  NOTIMPLEMENTED();
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), base::File::FILE_ERROR_FAILED));
}

void ArcContentFileSystemAsyncFileUtil::CreateSnapshotFile(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    CreateSnapshotFileCallback callback) {
  NOTIMPLEMENTED();
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), base::File::FILE_ERROR_FAILED,
                     base::File::Info(), base::FilePath(),
                     scoped_refptr<storage::ShareableFileReference>()));
}

}  // namespace arc
