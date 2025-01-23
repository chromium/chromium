// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/fileapi/arc_content_file_system_size_util.h"

#include "base/task/thread_pool.h"
#include "chrome/browser/ash/arc/fileapi/arc_file_system_operation_runner_util.h"
#include "chromeos/ash/experiences/arc/mojom/file_system.mojom-forward.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "url/gurl.h"

using content::BrowserThread;

namespace arc {

namespace {

using CloseStatus = file_system_operation_runner_util::CloseStatus;

void OnOpenFileToTruncate(int64_t length,
                          TruncateCallback callback,
                          mojom::FileSessionPtr file_handle) {
  if (file_handle.is_null() || file_handle->url_id.empty()) {
    std::move(callback).Run(base::File::FILE_ERROR_FAILED);
    return;
  }
  mojo::PlatformHandle platform_handle =
      mojo::UnwrapPlatformHandle(std::move(file_handle->fd));
  if (!platform_handle.is_valid()) {
    std::move(callback).Run(base::File::FILE_ERROR_FAILED);
    return;
  }
  base::File file(platform_handle.TakeFD());

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(
          [](base::File file, int64_t length) {
            return file.SetLength(length) ? base::File::FILE_OK
                                          : file.GetLastFileError();
          },
          std::move(file), length),
      base::BindOnce(
          [](const std::string& url_id, TruncateCallback callback,
             base::File::Error result) {
            const CloseStatus status = (result == base::File::FILE_OK)
                                           ? CloseStatus::kStatusOk
                                           : CloseStatus::kStatusError;
            file_system_operation_runner_util::CloseFileSession(url_id, status);
            std::move(callback).Run(result);
          },
          std::move(file_handle->url_id), std::move(callback)));
}

}  // namespace

void TruncateOnIOThread(const GURL& content_url,
                        int64_t length,
                        TruncateCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  file_system_operation_runner_util::OpenFileSessionToWriteOnIOThread(
      content_url,
      base::BindOnce(&OnOpenFileToTruncate, length, std::move(callback)));
}

}  // namespace arc
