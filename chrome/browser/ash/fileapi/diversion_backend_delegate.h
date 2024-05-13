// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILEAPI_DIVERSION_BACKEND_DELEGATE_H_
#define CHROME_BROWSER_ASH_FILEAPI_DIVERSION_BACKEND_DELEGATE_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/fileapi/diversion_file_manager.h"
#include "chrome/browser/ash/fileapi/file_system_backend_delegate.h"
#include "storage/browser/file_system/async_file_util.h"

namespace ash {

// A FileSystemBackendDelegate decorator (and, transitively, an AsyncFileUtil
// decorator) that combines its wrappees with a DiversionFileManager. It
// interposes a backed-by-local-disk cache (which also enables efficient
// incremental-append writes) for potentially-remote file systems.
//
// A DiversionBackendDelegate's methods should only be called from the
// content::BrowserThread::IO thread. Callbacks run on the same thread.
class DiversionBackendDelegate : public FileSystemBackendDelegate,
                                 public storage::AsyncFileUtil {
 public:
  // How this wrapper treats a virtual file (identified by its FileSystemURL).
  enum class Policy {
    kDoNotDivert,

    // Operations (EnsureFileExists, GetFileInfo, etc) happen entirely in the
    // interposed local-disk cache, and do not touch the wrappees, up until the
    // virtual file is copied or moved to another place (a place that, if also
    // subject to this DiversionBackendDelegate, its Policy should be
    // kDoNotDivert) or an inactivity time out.
    //
    // For example, calling GetFileInfo will return FILE_ERROR_NOT_FOUND
    // (unless EnsureFileExists was previously called) even if the wrapped file
    // system has an existing file for that name (that FileSystemURL). It
    // "doesn't exist" because the wrappees are not even consulted.
    //
    // Similarly, calling EnsureFileExists will return created=true even if the
    // wrappees have an existing file for that FileSystemURL.
    //
    // This Policy is intended for "temporary files", like "*.crdownload" or
    // "*.crswap", where a potentially-large file is incrementally built over
    // time before being moved/renamed over the ultimate destination. These
    // temporary files don't really care about their name other than it doesn't
    // clash with other files. But in Chromium's //storage/browser/file_system
    // cross-platform abstraction, every virtual file needs a unique name. With
    // DiversionBackendDelegate, we can provide an isolated "overlay namespace"
    // for these temporary files, ignoring the underlying wrapped file system.
    //
    // Isolation reduces the number of spurious calls to the wrappees. Spurious
    // work can fail (unnecessarily), take a noticeable amount of time (for
    // cloud-backed file systems), add noise to metrics or debug logs, etc.
    kDivertIsolated,

    // Operations happen in the interposed local-disk cache, but unlike
    // kDivertIsolated, they consult with the wrappee file system.
    //
    // For example, EnsureFileExists will return created=false when the wrappee
    // has an existing file for that FileSystemURL.
    //
    // This Policy is intended for when you do want diversion to apply (e.g.
    // you want efficient incremental-append writes) but you don't have a
    // "temporary file".
    //
    // If in doubt, use kDivertMingled instead of kDivertIsolated, as its
    // behavior is closer to not having a DiversionBackendDelegate at all.
    kDivertMingled,
  };

  explicit DiversionBackendDelegate(
      std::unique_ptr<FileSystemBackendDelegate> wrappee);
  ~DiversionBackendDelegate() override;

  DiversionBackendDelegate(const DiversionBackendDelegate&) = delete;
  DiversionBackendDelegate& operator=(const DiversionBackendDelegate&) = delete;

  // FileSystemBackendDelegate overrides.
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

  // storage::AsyncFileUtil overrides.
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

  void OverrideTmpfileDirForTesting(const base::FilePath& tmpfile_dir);
  static Policy ShouldDivertForTesting(const storage::FileSystemURL& url);
  static base::TimeDelta IdleTimeoutForTesting();

 private:
  enum class OnDiversionFinishedCallSite {
    kEnsureFileExists,
    kCopyFileLocal,
    kMoveFileLocal,
  };

  static void OnDiversionFinished(
      base::WeakPtr<DiversionBackendDelegate> weak_ptr,
      OnDiversionFinishedCallSite call_site,
      std::unique_ptr<storage::FileSystemOperationContext> context,
      const storage::FileSystemURL& dest_url,
      storage::AsyncFileUtil::StatusCallback callback,
      DiversionFileManager::StoppedReason stopped_reason,
      const storage::FileSystemURL& src_url,
      base::ScopedFD scoped_fd,
      int64_t file_size,
      base::File::Error error);

  std::unique_ptr<FileSystemBackendDelegate> wrappee_;
  scoped_refptr<DiversionFileManager> diversion_file_manager_;

  base::WeakPtrFactory<DiversionBackendDelegate> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_FILEAPI_DIVERSION_BACKEND_DELEGATE_H_
