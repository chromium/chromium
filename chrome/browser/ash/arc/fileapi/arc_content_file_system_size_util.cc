// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/fileapi/arc_content_file_system_size_util.h"

#include "base/task/thread_pool.h"
#include "chrome/browser/ash/arc/fileapi/arc_file_system_operation_runner_util.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "url/gurl.h"

using content::BrowserThread;

namespace arc {

namespace {

struct Result {
  base::File::Error error;
  int64_t size;
};

Result GetFileSizeFromFileHandle(mojo::ScopedHandle handle) {
  mojo::PlatformHandle platform_handle =
      mojo::UnwrapPlatformHandle(std::move(handle));
  if (!platform_handle.is_valid()) {
    return {base::File::FILE_ERROR_NOT_FOUND, -1};
  }
  base::ScopedPlatformFile file = platform_handle.TakePlatformFile();
  base::stat_wrapper_t info;
  if (base::File::Fstat(file.get(), &info) < 0 || !S_ISREG(info.st_mode)) {
    return {base::File::FILE_ERROR_FAILED, -1};
  } else {
    return {base::File::FILE_OK, info.st_size};
  }
}

void ReplyWithFileSizeFromHandleResult(GetFileSizeFromOpenFileCallback callback,
                                       Result result) {
  std::move(callback).Run(result.error, result.size);
}

void OnOpenFileToGetSize(GetFileSizeFromOpenFileCallback callback,
                         mojo::ScopedHandle handle) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&GetFileSizeFromFileHandle, std::move(handle)),
      base::BindOnce(&ReplyWithFileSizeFromHandleResult, std::move(callback)));
}

}  // namespace

void GetFileSizeFromOpenFileOnIOThread(
    GURL content_url,
    GetFileSizeFromOpenFileCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  file_system_operation_runner_util::OpenFileToReadOnIOThread(
      content_url, base::BindOnce(&OnOpenFileToGetSize, std::move(callback)));
}

void GetFileSizeFromOpenFileOnUIThread(
    GURL content_url,
    ArcFileSystemOperationRunner* runner,
    GetFileSizeFromOpenFileCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  runner->OpenFileToRead(
      content_url, base::BindOnce(&OnOpenFileToGetSize, std::move(callback)));
}

}  // namespace arc
