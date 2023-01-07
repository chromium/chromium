// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/active_directory_migration_utils.h"

#include "ash/constants/ash_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"

namespace ash {
namespace ad_migration_utils {

namespace {

constexpr char kChromadMigrationSkipOobePreservePath[] =
    "preserve/chromad_migration_skip_oobe";

}  // namespace

void CheckChromadMigrationOobeFlow(base::OnceCallback<void(bool)> callback) {
  base::FilePath preinstalled_components_dir;

  if (base::PathService::Get(DIR_PREINSTALLED_COMPONENTS,
                             &preinstalled_components_dir)) {
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE,
        {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
         base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
        base::BindOnce(&base::PathExists,
                       preinstalled_components_dir.AppendASCII(
                           kChromadMigrationSkipOobePreservePath)),
        std::move(callback));
  } else {
    LOG(WARNING) << "Failed to get the path to the preinstalled components. "
                    "The Welcome Screen won't be skipped in case this is part "
                    "of the Chromad migration flow.";
  }
}

}  // namespace ad_migration_utils
}  // namespace ash
