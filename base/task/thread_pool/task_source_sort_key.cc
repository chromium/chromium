// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/thread_pool/task_source_sort_key.h"

namespace base {
namespace internal {

static_assert(sizeof(TaskSourceSortKey) <= 2 * sizeof(uint64_t),
              "Members in TaskSourceSortKey should be ordered to be compact.");

TaskSourceSortKey::TaskSourceSortKey(TaskPriority priority,
                                     TimeTicks ready_time,
                                     uint8_t worker_count)
    : priority_(priority),
      worker_count_(worker_count),
      ready_time_(ready_time) {}

bool TaskSourceSortKey::operator<(const TaskSourceSortKey& other) const {
  // This TaskSourceSortKey is considered more important than |other| if it has
  // a higher priority or if it has the same priority but fewer workers, or if
  // it has the same priority and same worker count but its next task was
  // posted sooner than |other|'s.

  // A lower priority is considered less important.
  if (priority_ != other.priority_)
    return priority_ < other.priority_;

  // A greater worker count is considered less important.
  if (worker_count_ != other.worker_count_)
    return worker_count_ > other.worker_count_;

  // Lastly, a greater ready time is considered less important.
  return ready_time_ > other.ready_time_;
}

}  // namespace internal
}  // namespace base
