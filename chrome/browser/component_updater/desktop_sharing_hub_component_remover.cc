// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/desktop_sharing_hub_component_remover.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/task/thread_pool.h"
#include "components/component_updater/component_updater_utils.h"

namespace component_updater {

void DeleteDesktopSharingHub(const base::FilePath& user_data_dir) {
  base::ThreadPool::PostTask(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT, base::MayBlock()},
      base::BindOnce(
          &DeleteFilesAndParentDirectory,
          user_data_dir.Append(FILE_PATH_LITERAL("DesktopSharingHub"))));
}

}  // namespace component_updater
