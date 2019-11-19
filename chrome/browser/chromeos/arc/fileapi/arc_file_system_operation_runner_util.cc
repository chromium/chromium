// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/fileapi/arc_file_system_operation_runner_util.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/task/post_task.h"
#include "components/arc/arc_service_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "url/gurl.h"

using content::BrowserThread;

namespace arc {

namespace file_system_operation_runner_util {

namespace {

// TODO(crbug.com/745648): Use correct BrowserContext.
ArcFileSystemOperationRunner* GetArcFileSystemOperationRunner() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return ArcFileSystemOperationRunner::GetForBrowserContext(
      ArcServiceManager::Get()->browser_context());
}

template <typename T>
void PostToIOThread(base::OnceCallback<void(T)> callback, T result) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::PostTask(FROM_HERE, {BrowserThread::IO},
                 base::BindOnce(std::move(callback), std::move(result)));
}

void GetFileSizeOnUIThread(const GURL& url, GetFileSizeCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto* runner = GetArcFileSystemOperationRunner();
  if (!runner) {
    DLOG(ERROR) << "ArcFileSystemOperationRunner unavailable. "
                << "File system operations are dropped.";
    std::move(callback).Run(-1);
    return;
  }
  runner->GetFileSize(url, std::move(callback));
}

void OpenFileToReadOnUIThread(const GURL& url,
                              OpenFileToReadCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto* runner = GetArcFileSystemOperationRunner();
  if (!runner) {
    DLOG(ERROR) << "ArcFileSystemOperationRunner unavailable. "
                << "File system operations are dropped.";
    std::move(callback).Run(mojo::ScopedHandle());
    return;
  }
  runner->OpenFileToRead(url, std::move(callback));
}

void OpenFileToWriteOnUIThread(const GURL& url,
                               OpenFileToWriteCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto* runner = GetArcFileSystemOperationRunner();
  if (!runner) {
    DLOG(ERROR) << "ArcFileSystemOperationRunner unavailable. "
                << "File system operations are dropped.";
    std::move(callback).Run(mojo::ScopedHandle());
    return;
  }
  runner->OpenFileToWrite(url, std::move(callback));
}

}  // namespace

void GetFileSizeOnIOThread(const GURL& url, GetFileSizeCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  base::PostTask(FROM_HERE, {BrowserThread::UI},
                 base::BindOnce(&GetFileSizeOnUIThread, url,
                                base::BindOnce(&PostToIOThread<int64_t>,
                                               std::move(callback))));
}

void OpenFileToReadOnIOThread(const GURL& url,
                              OpenFileToReadCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  base::PostTask(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&OpenFileToReadOnUIThread, url,
                     base::BindOnce(&PostToIOThread<mojo::ScopedHandle>,
                                    std::move(callback))));
}

void OpenFileToWriteOnIOThread(const GURL& url,
                               OpenFileToReadCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  base::PostTask(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&OpenFileToWriteOnUIThread, url,
                     base::BindOnce(&PostToIOThread<mojo::ScopedHandle>,
                                    std::move(callback))));
}

}  // namespace file_system_operation_runner_util

}  // namespace arc
