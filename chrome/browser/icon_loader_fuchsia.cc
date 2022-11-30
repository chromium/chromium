// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(crbug.com/1226242): Implement support for downloads under Fuchsia.

#include "chrome/browser/icon_loader.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "ui/gfx/image/image.h"

// static
IconLoader::IconGroup IconLoader::GroupForFilepath(
    const base::FilePath& file_path) {
  NOTIMPLEMENTED_LOG_ONCE();
  return file_path.Extension();
}

// static
scoped_refptr<base::TaskRunner> IconLoader::GetReadIconTaskRunner() {
  NOTIMPLEMENTED_LOG_ONCE();
  return base::ThreadPool::CreateTaskRunner(traits());
}

void IconLoader::ReadIcon() {
  NOTIMPLEMENTED_LOG_ONCE();

  // Report back that no icon was found.
  target_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback_), gfx::Image(), group_));
  delete this;
}
