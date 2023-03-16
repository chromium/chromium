// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/task_executor.h"

#include <type_traits>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/task/task_traits.h"
#include "base/task/task_traits_extension.h"
#include "third_party/abseil-cpp/absl/base/attributes.h"

namespace base {

namespace {

// Maps TaskTraits extension IDs to registered TaskExecutors. Index |n|
// corresponds to id |n - 1|.
using TaskExecutorMap =
    std::array<TaskExecutor*, TaskTraitsExtensionStorage::kMaxExtensionId>;
TaskExecutorMap* GetTaskExecutorMap() {
  static_assert(std::is_trivially_destructible<TaskExecutorMap>::value,
                "TaskExecutorMap not trivially destructible");
  static TaskExecutorMap executors{};
  return &executors;
}

static_assert(
    TaskTraitsExtensionStorage::kInvalidExtensionId == 0,
    "TaskExecutorMap depends on 0 being an invalid TaskTraits extension ID");

ABSL_CONST_INIT thread_local TaskExecutor* current_task_executor = nullptr;

}  // namespace

void SetTaskExecutorForCurrentThread(TaskExecutor* task_executor) {
  DCHECK(!task_executor || !GetTaskExecutorForCurrentThread() ||
         GetTaskExecutorForCurrentThread() == task_executor);
  current_task_executor = task_executor;
}

TaskExecutor* GetTaskExecutorForCurrentThread() {
  // Workaround false-positive MSAN use-of-uninitialized-value on
  // thread_local storage for loaded libraries:
  // https://github.com/google/sanitizers/issues/1265
  MSAN_UNPOISON(&current_task_executor, sizeof(TaskExecutor*));

  return current_task_executor;
}

void RegisterTaskExecutor(uint8_t extension_id, TaskExecutor* task_executor) {
  DCHECK_NE(extension_id, TaskTraitsExtensionStorage::kInvalidExtensionId);
  DCHECK_LE(extension_id, TaskTraitsExtensionStorage::kMaxExtensionId);
  DCHECK_EQ((*GetTaskExecutorMap())[extension_id - 1], nullptr);

  (*GetTaskExecutorMap())[extension_id - 1] = task_executor;
}

void UnregisterTaskExecutorForTesting(uint8_t extension_id) {
  DCHECK_NE(extension_id, TaskTraitsExtensionStorage::kInvalidExtensionId);
  DCHECK_LE(extension_id, TaskTraitsExtensionStorage::kMaxExtensionId);
  DCHECK_NE((*GetTaskExecutorMap())[extension_id - 1], nullptr);

  (*GetTaskExecutorMap())[extension_id - 1] = nullptr;
}

TaskExecutor* GetRegisteredTaskExecutorForTraits(const TaskTraits& traits) {
  uint8_t extension_id = traits.extension_id();
  if (extension_id != TaskTraitsExtensionStorage::kInvalidExtensionId) {
    TaskExecutor* executor = (*GetTaskExecutorMap())[extension_id - 1];
    DCHECK(executor)
        << "A TaskExecutor wasn't yet registered for this extension.\nHint: if "
           "this is in a unit test, you're likely missing a "
           "content::BrowserTaskEnvironment member in your fixture.";
    return executor;
  }

  return nullptr;
}

}  // namespace base
