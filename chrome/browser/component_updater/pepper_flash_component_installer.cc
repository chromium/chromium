// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/pepper_flash_component_installer.h"

#include <vector>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/component_updater/component_updater_paths.h"

namespace component_updater {

void CleanUpPepperFlashComponent(const base::FilePath& profile_path) {
  std::vector<base::FilePath> delete_dirs;

  base::FilePath component_dir;
  if (base::PathService::Get(DIR_COMPONENT_USER, &component_dir))
    delete_dirs.push_back(component_dir.AppendASCII("PepperFlash"));

  delete_dirs.push_back(
      profile_path.AppendASCII("Pepper Data").AppendASCII("Shockwave Flash"));
  base::ThreadPool::PostTask(FROM_HERE, {base::MayBlock()},
                             base::BindOnce(
                                 [](const std::vector<base::FilePath>& dirs) {
                                   for (const base::FilePath& dir : dirs) {
                                     base::DeletePathRecursively(dir);
                                   }
                                 },
                                 delete_dirs));
}

}  // namespace component_updater
