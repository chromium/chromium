// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/fileapi/arc_file_system_operation_runner_util.h"

#include <utility>
#include <vector>

#include "ash/components/arc/mojom/file_system.mojom-forward.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "base/functional/bind.h"
#include "base/task/bind_post_task.h"
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
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(result)));
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

void OpenFileSessionToWriteOnUIThread(const GURL& url,
                                      OpenFileSessionToWriteCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto* runner = GetArcFileSystemOperationRunner();
  if (!runner) {
    DLOG(ERROR) << "ArcFileSystemOperationRunner unavailable. "
                << "File system operations are dropped.";
    std::move(callback).Run(mojom::FileSessionPtr());
    return;
  }
  runner->OpenFileSessionToWrite(url, std::move(callback));
}

void OpenFileSessionToReadOnUIThread(const GURL& url,
                                     OpenFileSessionToReadCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto* runner = GetArcFileSystemOperationRunner();
  if (!runner) {
    DLOG(ERROR) << "ArcFileSystemOperationRunner unavailable. "
                << "File system operations are dropped.";
    std::move(callback).Run(mojom::FileSessionPtr());
    return;
  }
  runner->OpenFileSessionToRead(url, std::move(callback));
}

void CloseFileSessionOnUIThread(const std::string& url_id,
                                const CloseStatus status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto* runner = GetArcFileSystemOperationRunner();
  if (!runner) {
    DLOG(ERROR) << "ArcFileSystemOperationRunner unavailable. "
                << "File system operations are dropped.";
    return;
  }
  std::string message;
  if (status == CloseStatus::kStatusError) {
    message = "file operation exited with error";
  } else if (status == CloseStatus::kStatusCancel) {
    message = "file operation was cancelled";
  }
  runner->CloseFileSession(url_id, message);
}

}  // namespace

void GetFileSizeOnIOThread(const GURL& url, GetFileSizeCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&GetFileSizeOnUIThread, url,
                                base::BindOnce(&PostToIOThread<int64_t>,
                                               std::move(callback))));
}

void OpenFileSessionToWriteOnIOThread(const GURL& url,
                                      OpenFileSessionToWriteCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&OpenFileSessionToWriteOnUIThread, url,
                     base::BindPostTask(content::GetIOThreadTaskRunner({}),
                                        std::move(callback))));
}

void OpenFileSessionToReadOnIOThread(const GURL& url,
                                     OpenFileSessionToReadCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&OpenFileSessionToReadOnUIThread, url,
                     base::BindOnce(&PostToIOThread<mojom::FileSessionPtr>,
                                    std::move(callback))));
}

// TODO(b/222823695): Consider using a mojo interface to disconnect remote.
void CloseFileSession(const std::string& url_id, const CloseStatus status) {
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&CloseFileSessionOnUIThread, url_id, status));
}

}  // namespace file_system_operation_runner_util

}  // namespace arc
