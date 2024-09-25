// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/snapshot_manager.h"

#include <utility>

#include "base/containers/circular_deque.h"
#include "base/files/file.h"
#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "base/system/sys_info.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/common/task_util.h"
#include "storage/browser/blob/shareable_file_reference.h"
#include "storage/browser/file_system/file_system_backend.h"
#include "storage/browser/file_system/file_system_context.h"
#include "third_party/cros_system_api/constants/cryptohome.h"

namespace file_manager {
namespace {

typedef base::OnceCallback<void(int64_t)> GetNecessaryFreeSpaceCallback;

// Part of ComputeSpaceNeedToBeFreed.
int64_t ComputeSpaceNeedToBeFreedAfterGetMetadataAsync(
    const base::FilePath& path,
    int64_t snapshot_size) {
  int64_t free_size = base::SysInfo::AmountOfFreeDiskSpace(path);
  if (free_size < 0) {
    return -1;
  }

  // We need to keep cryptohome::kMinFreeSpaceInBytes free space even after
  // |snapshot_size| is occupied.
  free_size -= snapshot_size + cryptohome::kMinFreeSpaceInBytes;
  return (free_size < 0 ? -free_size : 0);
}

// Part of ComputeSpaceNeedToBeFreed.
void ComputeSpaceNeedToBeFreedAfterGetMetadata(
    const base::FilePath& path,
    GetNecessaryFreeSpaceCallback callback,
    base::File::Error result,
    const base::File::Info& file_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (result != base::File::FILE_OK) {
    std::move(callback).Run(-1);
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::BindOnce(&ComputeSpaceNeedToBeFreedAfterGetMetadataAsync, path,
                     file_info.size),
      std::move(callback));
}

// Part of ComputeSpaceNeedToBeFreed.
void GetMetadataOnIOThread(const base::FilePath& path,
                           scoped_refptr<storage::FileSystemContext> context,
                           const storage::FileSystemURL& url,
                           GetNecessaryFreeSpaceCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  context->operation_runner()->GetMetadata(
      url, {storage::FileSystemOperation::GetMetadataField::kSize},
      base::BindOnce(&ComputeSpaceNeedToBeFreedAfterGetMetadata, path,
                     std::move(callback)));
}

// Computes the size of space that need to be __additionally__ made available
// in the |profile|'s data directory for taking the snapshot of |url|.
// Returns 0 if no additional space is required, or -1 in the case of an error.
void ComputeSpaceNeedToBeFreed(
    Profile* profile,
    scoped_refptr<storage::FileSystemContext> context,
    const storage::FileSystemURL& url,
    GetNecessaryFreeSpaceCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&GetMetadataOnIOThread, profile->GetPath(), context, url,
                     google_apis::CreateRelayCallback(std::move(callback))));
}

}  // namespace

class SnapshotManager::FileRefsHolder
    : public base::RefCountedThreadSafe<
          SnapshotManager::FileRefsHolder,
          content::BrowserThread::DeleteOnIOThread> {
 public:
  FileRefsHolder() = default;

  FileRefsHolder(const FileRefsHolder&) = delete;
  FileRefsHolder& operator=(const FileRefsHolder&) = delete;

  void FreeSpaceAndCreateSnapshotFile(
      scoped_refptr<storage::FileSystemContext> context,
      const storage::FileSystemURL& url,
      int64_t needed_space,
      LocalPathCallback callback);

  void OnCreateSnapshotFile(
      LocalPathCallback callback,
      base::File::Error result,
      const base::File::Info& file_info,
      const base::FilePath& platform_path,
      scoped_refptr<storage::ShareableFileReference> file_ref);

 private:
  // Struct for keeping the snapshot file reference with its file size used for
  // computing the necessity of clean up.
  struct FileReferenceWithSizeInfo {
    FileReferenceWithSizeInfo(
        scoped_refptr<storage::ShareableFileReference> ref,
        int64_t size)
        : file_ref(ref), file_size(size) {}
    FileReferenceWithSizeInfo(const FileReferenceWithSizeInfo& other) = default;
    ~FileReferenceWithSizeInfo() = default;

    scoped_refptr<storage::ShareableFileReference> file_ref;
    int64_t file_size;
  };

  friend struct content::BrowserThread::DeleteOnThread<
      content::BrowserThread::IO>;
  friend class base::DeleteHelper<FileRefsHolder>;

  ~FileRefsHolder() = default;

  base::circular_deque<FileReferenceWithSizeInfo> file_refs_;
};

void SnapshotManager::FileRefsHolder::FreeSpaceAndCreateSnapshotFile(
    scoped_refptr<storage::FileSystemContext> context,
    const storage::FileSystemURL& url,
    int64_t needed_space,
    LocalPathCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (needed_space < 0) {
    std::move(callback).Run(base::FilePath());
    return;
  }

  // Free up to the required size.
  while (needed_space > 0 && !file_refs_.empty()) {
    needed_space -= file_refs_.front().file_size;
    file_refs_.pop_front();
  }

  // If we still could not achieve the space requirement, abort with failure.
  if (needed_space > 0) {
    std::move(callback).Run(base::FilePath());
    return;
  }

  context->operation_runner()->CreateSnapshotFile(
      url, base::BindOnce(&FileRefsHolder::OnCreateSnapshotFile, this,
                          std::move(callback)));
}

void SnapshotManager::FileRefsHolder::OnCreateSnapshotFile(
    LocalPathCallback callback,
    base::File::Error result,
    const base::File::Info& file_info,
    const base::FilePath& platform_path,
    scoped_refptr<storage::ShareableFileReference> file_ref) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (result != base::File::FILE_OK) {
    std::move(callback).Run(base::FilePath());
    return;
  }

  file_refs_.push_back(
      FileReferenceWithSizeInfo(std::move(file_ref), file_info.size));
  std::move(callback).Run(platform_path);
}

SnapshotManager::SnapshotManager(Profile* profile)
    : profile_(profile), holder_(base::MakeRefCounted<FileRefsHolder>()) {}

SnapshotManager::~SnapshotManager() = default;

void SnapshotManager::CreateManagedSnapshot(
    const base::FilePath& absolute_file_path,
    LocalPathCallback callback) {
  scoped_refptr<storage::FileSystemContext> context(
      util::GetFileManagerFileSystemContext(profile_));
  DCHECK(context.get());

  GURL url;
  if (!util::ConvertAbsoluteFilePathToFileSystemUrl(
          profile_, absolute_file_path, util::GetFileManagerURL(), &url)) {
    std::move(callback).Run(base::FilePath());
    return;
  }
  storage::FileSystemURL filesystem_url =
      context->CrackURLInFirstPartyContext(url);

  ComputeSpaceNeedToBeFreed(
      profile_, context, filesystem_url,
      base::BindOnce(&SnapshotManager::CreateManagedSnapshotAfterSpaceComputed,
                     weak_ptr_factory_.GetWeakPtr(), filesystem_url,
                     std::move(callback)));
}

void SnapshotManager::CreateManagedSnapshotAfterSpaceComputed(
    const storage::FileSystemURL& filesystem_url,
    LocalPathCallback callback,
    int64_t needed_space) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  scoped_refptr<storage::FileSystemContext> context(
      util::GetFileManagerFileSystemContext(profile_));
  DCHECK(context.get());

  // Free up space if needed and start creating the snapshot.
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&FileRefsHolder::FreeSpaceAndCreateSnapshotFile, holder_,
                     context, filesystem_url, needed_space,
                     google_apis::CreateRelayCallback(std::move(callback))));
}

}  // namespace file_manager
