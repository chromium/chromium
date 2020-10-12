// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/pepper_flash_component_installer.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/component_updater/component_updater_paths.h"

namespace component_updater {

void CleanUpPepperFlashComponent() {
  base::FilePath component_dir;
  if (!base::PathService::Get(DIR_COMPONENT_USER, &component_dir))
    return;
  base::ThreadPool::PostTask(FROM_HERE, {base::MayBlock()},
                             base::BindOnce(
                                 [](const base::FilePath& dir) -> void {
                                   base::DeletePathRecursively(dir);
                                 },
                                 component_dir.AppendASCII("PepperFlash")));
}

}  // namespace component_updater
