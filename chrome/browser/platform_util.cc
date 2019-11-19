// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/platform_util.h"

#include "base/bind.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/task/post_task.h"
#include "chrome/browser/platform_util_internal.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace platform_util {

namespace {

bool shell_operations_allowed = true;

void VerifyAndOpenItemOnBlockingThread(const base::FilePath& path,
                                       OpenItemType type,
                                       const OpenOperationCallback& callback) {
  base::File target_item(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!base::PathExists(path)) {
    if (!callback.is_null())
      base::PostTask(FROM_HERE, {BrowserThread::UI},
                     base::BindOnce(callback, OPEN_FAILED_PATH_NOT_FOUND));
    return;
  }
  if (base::DirectoryExists(path) != (type == OPEN_FOLDER)) {
    if (!callback.is_null())
      base::PostTask(FROM_HERE, {BrowserThread::UI},
                     base::BindOnce(callback, OPEN_FAILED_INVALID_TYPE));
    return;
  }

  if (shell_operations_allowed)
    internal::PlatformOpenVerifiedItem(path, type);
  if (!callback.is_null())
    base::PostTask(FROM_HERE, {BrowserThread::UI},
                   base::BindOnce(callback, OPEN_SUCCEEDED));
}

}  // namespace

namespace internal {

void DisableShellOperationsForTesting() {
  shell_operations_allowed = false;
}

}  // namespace internal

void OpenItem(Profile* profile,
              const base::FilePath& full_path,
              OpenItemType item_type,
              const OpenOperationCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // TaskPriority::USER_BLOCKING because this is usually opened as a result of a
  // user action (e.g. open-downloaded-file or show-item-in-folder).
  // TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN because this doesn't need global
  // state and can hang shutdown without this trait as it may result in an
  // interactive dialog.
  base::PostTask(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::USER_BLOCKING,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&VerifyAndOpenItemOnBlockingThread, full_path, item_type,
                     callback));
}

bool IsBrowserLockedFullscreen(const Browser* browser) {
  return false;
}

}  // namespace platform_util
