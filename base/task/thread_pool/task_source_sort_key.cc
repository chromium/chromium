// Copyright 2016 The Chromium Authors. All rights reserved.
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

bool TaskSourceSortKey::operator<=(const TaskSourceSortKey& other) const {
  // This TaskSourceSortKey is considered more important than |other| if it has
  // a higher priority or if it has the same priority but fewer workers, or if
  // it has the same priority and same worker count but its next task was
  // posted sooner than |other|'s.
  const int priority_diff =
      static_cast<int>(priority_) - static_cast<int>(other.priority_);
  if (priority_diff > 0)
    return true;
  if (priority_diff < 0)
    return false;
  if (worker_count_ < other.worker_count_)
    return true;
  if (worker_count_ > other.worker_count_)
    return false;
  return ready_time_ <= other.ready_time_;
}

}  // namespace internal
}  // namespace base
