// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/smb_client/fileapi/smbfs_async_file_util.h"

#include <utility>

#include "base/check_op.h"
#include "base/files/file.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/ash/smb_client/smb_service.h"
#include "chrome/browser/ash/smb_client/smb_service_factory.h"
#include "chrome/browser/ash/smb_client/smbfs_share.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/file_system/local_file_util.h"

namespace ash::smb_client {
namespace {

void AllowCredentialsRequestOnUIThread(Profile* profile,
                                       const base::FilePath& path) {
  SmbService* service = SmbServiceFactory::Get(profile);
  DCHECK(service);
  SmbFsShare* share = service->GetSmbFsShareForPath(path);
  // Because the request is posted from the IO thread, there's no guarantee the
  // share still exists at this point.
  if (share) {
    // To avoid spamming the user with credentials dialogs, we only want to show
    // the dialog when the user clicks on the share in the Files App. However,
    // there's no way to know the request came from the Files App. Instead,
    // intercept ReadDirectory(), which the Files App does whenever the user
    // enters a directory and use that as a proxy for user-initiated navigation.
    // This isn't perfect, since lots of other things are likely to ask for a
    // directory listing. But it also prevents dialog activation by any
    // operation done purely through the native FUSE filesystem.
    share->AllowCredentialsRequest();
  }
}

class DeleteRecursivelyOperation {
 public:
  DeleteRecursivelyOperation(
      Profile* profile,
      const base::FilePath& path,
      storage::AsyncFileUtil::StatusCallback callback,
      scoped_refptr<base::SequencedTaskRunner> origin_task_runner)
      : profile_(profile),
        path_(path),
        callback_(std::move(callback)),
        origin_task_runner_(std::move(origin_task_runner)) {
    DCHECK(origin_task_runner_->RunsTasksInCurrentSequence());
  }

  DeleteRecursivelyOperation() = delete;
  DeleteRecursivelyOperation(const DeleteRecursivelyOperation&) = delete;
  DeleteRecursivelyOperation& operator=(const DeleteRecursivelyOperation&) =
      delete;

  void Start() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    SmbService* service = SmbServiceFactory::Get(profile_);
    DCHECK(service);
    SmbFsShare* share = service->GetSmbFsShareForPath(path_);

    // Because the request is posted from the IO thread, there's no guarantee
    // the share still exists at this point.
    if (!share) {
      origin_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(callback_), base::File::FILE_ERROR_FAILED));
      delete this;
      return;
    }

    share->DeleteRecursively(
        path_, base::BindOnce(&DeleteRecursivelyOperation::OnDeleteRecursively,
                              base::Owned(this)));
  }

  void OnDeleteRecursively(base::File::Error error) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    // Make StatusCallback on the thread where the operation originated.
    origin_task_runner_->PostTask(FROM_HERE,
                                  base::BindOnce(std::move(callback_), error));
  }

  const raw_ptr<Profile> profile_;
  const base::FilePath path_;
  storage::AsyncFileUtil::StatusCallback callback_;
  scoped_refptr<base::SequencedTaskRunner> origin_task_runner_;
};

}  // namespace

SmbFsAsyncFileUtil::SmbFsAsyncFileUtil(Profile* profile)
    : AsyncFileUtilAdapter(std::make_unique<storage::LocalFileUtil>()),
      profile_(profile) {
  DCHECK(profile_);
}

SmbFsAsyncFileUtil::~SmbFsAsyncFileUtil() = default;

void SmbFsAsyncFileUtil::ReadDirectory(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    ReadDirectoryCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  content::GetUIThreadTaskRunner({})->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&AllowCredentialsRequestOnUIThread, profile_, url.path()),
      base::BindOnce(&SmbFsAsyncFileUtil::RealReadDirectory,
                     weak_factory_.GetWeakPtr(), std::move(context), url,
                     std::move(callback)));
}

void SmbFsAsyncFileUtil::RealReadDirectory(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    ReadDirectoryCallback callback) {
  storage::AsyncFileUtilAdapter::ReadDirectory(std::move(context), url,
                                               std::move(callback));
}

void SmbFsAsyncFileUtil::DeleteRecursively(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    StatusCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&DeleteRecursivelyOperation::Start,
                     base::Unretained(new DeleteRecursivelyOperation(
                         profile_, url.path(), std::move(callback),
                         base::SequencedTaskRunner::GetCurrentDefault()))));
}

}  // namespace ash::smb_client
