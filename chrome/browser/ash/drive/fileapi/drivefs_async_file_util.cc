// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/drive/fileapi/drivefs_async_file_util.h"

#include <utility>

#include "ash/constants/ash_features.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/drive/file_system_util.h"
#include "components/drive/file_errors.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_operation.h"
#include "storage/browser/file_system/file_system_operation_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/file_system/local_file_util.h"
#include "storage/browser/file_system/native_file_util.h"
#include "storage/common/file_system/file_system_util.h"

namespace drive::internal {
namespace {

class DriveFsFileUtil : public storage::LocalFileUtil {
 public:
  DriveFsFileUtil() = default;

  DriveFsFileUtil(const DriveFsFileUtil&) = delete;
  DriveFsFileUtil& operator=(const DriveFsFileUtil&) = delete;

  ~DriveFsFileUtil() override = default;

 protected:
  bool IsHiddenItem(const base::FilePath& local_file_path) const override {
    // DriveFS is a trusted filesystem, allow symlinks.
    return false;
  }
};

class CopyOperation : public base::RefCountedThreadSafe<CopyOperation> {
 public:
  CopyOperation(
      Profile* const profile,
      std::unique_ptr<storage::FileSystemOperationContext> context,
      storage::FileSystemURL src_url,
      storage::FileSystemURL dest_url,
      storage::AsyncFileUtil::CopyOrMoveOptionSet options,
      storage::AsyncFileUtil::CopyFileProgressCallback progress_callback,
      storage::AsyncFileUtil::StatusCallback callback,
      scoped_refptr<base::SequencedTaskRunner> origin_task_runner,
      base::WeakPtr<DriveFsAsyncFileUtil> async_file_util)
      : profile_(profile),
        context_(std::move(context)),
        src_url_(std::move(src_url)),
        dest_url_(std::move(dest_url)),
        options_(std::move(options)),
        progress_callback_(std::move(progress_callback)),
        callback_(std::move(callback)),
        origin_task_runner_(std::move(origin_task_runner)),
        async_file_util_(std::move(async_file_util)) {
    DCHECK(origin_task_runner_->RunsTasksInCurrentSequence());
  }

  void Start() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    auto* drive_integration_service =
        drive::util::GetIntegrationServiceByProfile(profile_);
    base::FilePath source_path("/");
    base::FilePath destination_path("/");
    if (!drive_integration_service ||
        !drive_integration_service->GetMountPointPath().AppendRelativePath(
            src_url_.path(), &source_path) ||
        !drive_integration_service->GetMountPointPath().AppendRelativePath(
            dest_url_.path(), &destination_path)) {
      origin_task_runner_->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback_),
                                    base::File::FILE_ERROR_INVALID_OPERATION));
      return;
    }
    drive_integration_service->GetDriveFsInterface()->CopyFile(
        source_path, destination_path,
        mojo::WrapCallbackWithDefaultInvokeIfNotRun(
            base::BindOnce(&CopyOperation::CopyComplete, this),
            drive::FILE_ERROR_ABORT));
  }

 private:
  friend class base::RefCountedThreadSafe<CopyOperation>;
  ~CopyOperation() = default;

  void CopyComplete(drive::FileError error) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    switch (error) {
      case drive::FILE_ERROR_NOT_FOUND:
      case drive::FILE_ERROR_NO_CONNECTION:
        origin_task_runner_->PostTask(
            FROM_HERE,
            base::BindOnce(&CopyOperation::FallbackToNativeCopyOnOriginThread,
                           this));
        break;

      default:
        origin_task_runner_->PostTask(
            FROM_HERE, base::BindOnce(std::move(callback_),
                                      FileErrorToBaseFileError(error)));
    }
  }

  void FallbackToNativeCopyOnOriginThread() {
    DCHECK(origin_task_runner_->RunsTasksInCurrentSequence());

    if (!async_file_util_) {
      std::move(callback_).Run(base::File::FILE_ERROR_ABORT);
      return;
    }
    async_file_util_->AsyncFileUtilAdapter::CopyFileLocal(
        std::move(context_), src_url_, dest_url_, options_,
        std::move(progress_callback_), std::move(callback_));
  }

  const raw_ptr<Profile> profile_;
  std::unique_ptr<storage::FileSystemOperationContext> context_;
  const storage::FileSystemURL src_url_;
  const storage::FileSystemURL dest_url_;
  const storage::AsyncFileUtil::CopyOrMoveOptionSet options_;
  storage::AsyncFileUtil::CopyFileProgressCallback progress_callback_;
  storage::AsyncFileUtil::StatusCallback callback_;
  const scoped_refptr<base::SequencedTaskRunner> origin_task_runner_;
  const base::WeakPtr<DriveFsAsyncFileUtil> async_file_util_;
};

// Recursively deletes a folder locally. The folder will still be available in
// Drive cloud Trash.
class DeleteOperation : public base::RefCountedThreadSafe<DeleteOperation> {
 public:
  using PinningManager = drivefs::pinning::PinningManager;
  using Id = PinningManager::Id;

  DeleteOperation(Profile* const profile,
                  base::FilePath path,
                  storage::AsyncFileUtil::StatusCallback callback,
                  scoped_refptr<base::SequencedTaskRunner> origin_task_runner,
                  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner)
      : profile_(profile),
        path_(std::move(path)),
        callback_(std::move(callback)),
        origin_task_runner_(std::move(origin_task_runner)),
        blocking_task_runner_(std::move(blocking_task_runner)) {
    DCHECK(origin_task_runner_->RunsTasksInCurrentSequence());
  }

  void Start() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    DCHECK(!drive_);
    drive_ = drive::util::GetIntegrationServiceByProfile(profile_);
    base::FilePath relative_path;
    if (!drive_ || !drive_->GetMountPointPath().IsParent(path_)) {
      origin_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(callback_), base::File::FILE_ERROR_FAILED));
      return;
    }

    if (drive_->GetPinningManager() &&
        drive_->GetRelativeDrivePath(path_, &drive_path_)) {
      // TODO(b/266168982): In the case this is a folder, only the folder will
      // get unpinned leaving all the children pinned. When the new method is
      // exposed (or parameter on the existing method) update the
      // implementation here.
      drive_->GetDriveFsInterface()->GetMetadata(
          drive_path_, base::BindOnce(&DeleteOperation::OnGotMetadata, this));
      return;
    }

    blocking_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&DeleteOperation::Delete, this));
  }

 private:
  friend class base::RefCountedThreadSafe<DeleteOperation>;
  ~DeleteOperation() = default;

  void OnGotMetadata(const drive::FileError error,
                     const drivefs::mojom::FileMetadataPtr metadata) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    if (error != drive::FILE_ERROR_OK) {
      LOG(ERROR) << "Cannot get metadata of '" << drive_path_ << "': " << error;
    } else {
      DCHECK(metadata);
      id_ = Id(metadata->stable_id);
    }

    blocking_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&DeleteOperation::Delete, this));
  }

  void Delete() {
    VLOG(1) << "Deleting '" << path_ << "'...";
    const bool deleted = base::DeletePathRecursively(path_);

    if (deleted) {
      VLOG(1) << "Deleted '" << path_ << "'";
      content::GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE, base::BindOnce(&DeleteOperation::OnDeleted, this));
    } else {
      LOG(ERROR) << "Cannot delete '" << path_ << "'";
    }

    origin_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback_),
                                  deleted ? base::File::FILE_OK
                                          : base::File::FILE_ERROR_FAILED));
  }

  void OnDeleted() {
    DCHECK(drive_);
    if (PinningManager* const pinning_manager = drive_->GetPinningManager()) {
      // TODO(b/267225898) Local delete events are currently not sent via
      // DriveFS, so for now we notify the `PinningManager` for local deletes.
      pinning_manager->NotifyDelete(id_, drive_path_);
    }
  }

  const raw_ptr<Profile> profile_;
  const base::FilePath path_;
  base::FilePath drive_path_;
  Id id_ = Id::kNone;
  storage::AsyncFileUtil::StatusCallback callback_;
  const scoped_refptr<base::SequencedTaskRunner> origin_task_runner_;
  const scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;

  raw_ptr<drive::DriveIntegrationService> drive_ = nullptr;
};

}  // namespace

DriveFsAsyncFileUtil::DriveFsAsyncFileUtil(Profile* profile)
    : AsyncFileUtilAdapter(std::make_unique<DriveFsFileUtil>()),
      profile_(profile) {}

DriveFsAsyncFileUtil::~DriveFsAsyncFileUtil() = default;

void DriveFsAsyncFileUtil::CopyFileLocal(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& src_url,
    const storage::FileSystemURL& dest_url,
    CopyOrMoveOptionSet options,
    CopyFileProgressCallback progress_callback,
    StatusCallback callback) {
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&CopyOperation::Start,
                     base::MakeRefCounted<CopyOperation>(
                         profile_, std::move(context), src_url, dest_url,
                         std::move(options), std::move(progress_callback),
                         std::move(callback),
                         base::SequencedTaskRunner::GetCurrentDefault(),
                         weak_factory_.GetWeakPtr())));
}

void DriveFsAsyncFileUtil::DeleteRecursively(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    StatusCallback callback) {
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&DeleteOperation::Start,
                     base::MakeRefCounted<DeleteOperation>(
                         profile_, url.path(), std::move(callback),
                         base::SequencedTaskRunner::GetCurrentDefault(),
                         context->task_runner())));
}

}  // namespace drive::internal
