// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/lacros_component_remover.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"

namespace component_updater {

void DeleteStatefulLacros(const base::FilePath& user_data_dir) {
  scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT});
  sequenced_task_runner->PostTask(
      FROM_HERE, base::BindOnce(
                     [](const base::FilePath& user_data_dir) {
                       base::DeletePathRecursively(user_data_dir.Append(
                           FILE_PATH_LITERAL("lacros-dogfood-canary")));
                       base::DeletePathRecursively(user_data_dir.Append(
                           FILE_PATH_LITERAL("lacros-dogfood-dev")));
                       base::DeletePathRecursively(user_data_dir.Append(
                           FILE_PATH_LITERAL("lacros-dogfood-beta")));
                       base::DeletePathRecursively(user_data_dir.Append(
                           FILE_PATH_LITERAL("lacros-dogfood-stable")));
                     },
                     user_data_dir));
}

}  // namespace component_updater
