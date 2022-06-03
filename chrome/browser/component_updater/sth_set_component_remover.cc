// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/sth_set_component_remover.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/component_updater/component_updater_utils.h"

namespace component_updater {

void DeleteLegacySTHSet(const base::FilePath& user_data_dir) {
  base::ThreadPool::PostTask(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT, base::MayBlock()},
      base::BindOnce(
          &DeleteFilesAndParentDirectory,
          user_data_dir.Append(FILE_PATH_LITERAL("CertificateTransparency"))));
}

}  // namespace component_updater
