// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/remove_stale_data.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/base_tracing.h"

namespace base {
namespace android {

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class DeleteResult {
  kNotFound = 0,
  kDeleted = 1,
  kDeleteError = 2,
  kMaxValue = kDeleteError,
};

void RecordDeleteResult(DeleteResult result) {
  base::UmaHistogramEnumeration("NetworkService.ClearStaleDataDirectoryResult",
                                result);
}

void RemoveStaleDataDirectoryOnPool(const base::FilePath& data_directory) {
  TRACE_EVENT0("startup", "RemoveStaleDataDirectoryOnPool");
  if (!base::PathExists(data_directory)) {
    RecordDeleteResult(DeleteResult::kNotFound);
    return;
  }
  if (base::DeletePathRecursively(data_directory)) {
    RecordDeleteResult(DeleteResult::kDeleted);
    return;
  }
  RecordDeleteResult(DeleteResult::kDeleteError);
}

}  // namespace

void RemoveStaleDataDirectory(const base::FilePath& data_directory) {
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::TaskPriority::BEST_EFFORT, base::MayBlock(),
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&RemoveStaleDataDirectoryOnPool, data_directory));
}

}  // namespace android
}  // namespace base
