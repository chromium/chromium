// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/icon_loader.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/task/thread_pool.h"
#include "ui/gfx/image/image.h"

// static
IconLoader::IconGroup IconLoader::GroupForFilepath(
    const base::FilePath& file_path) {
  // Like Windows and Linux, all files with the same extension share an icon.
  return file_path.Extension();
}

// static
scoped_refptr<base::TaskRunner> IconLoader::GetReadIconTaskRunner() {
  // ReadIcon() does not touch any thread-affine state on Android, so any
  // thread pool sequence will do.
  return base::ThreadPool::CreateTaskRunner(traits());
}

void IconLoader::ReadIcon() {
  // Android has no OS-provided file type icons that are accessible from here,
  // so report failure with an empty image. Callers such as
  // chrome.downloads.getFileIcon() already treat an empty image as "icon not
  // found".
  // TODO(crbug.com/579516055): Provide Chrome's own file type icons here.
  target_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback_), gfx::Image(), group_));
  delete this;
}
