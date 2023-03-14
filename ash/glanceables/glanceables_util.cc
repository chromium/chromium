// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/glanceables/glanceables_util.h"

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/path_service.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"

namespace ash::glanceables_util {

base::FilePath GetSignoutScreenshotPath() {
  base::FilePath home_dir;
  CHECK(base::PathService::Get(base::DIR_HOME, &home_dir));
  return home_dir.AppendASCII("signout_screenshot.png");
}

void DeleteScreenshot() {
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::LOWEST},
      base::BindOnce(base::IgnoreResult(&base::DeleteFile),
                     glanceables_util::GetSignoutScreenshotPath()));
}

}  // namespace ash::glanceables_util
