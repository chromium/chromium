// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RASTER_TASK_CATEGORY_H_
#define CC_RASTER_TASK_CATEGORY_H_

#include <cstdint>

namespace cc {

// This enum provides values for TaskGraph::Node::category, which is a uint16_t.
// We don't use an enum class here, as we want to keep TaskGraph::Node::category
// generic, allowing other consumers to provide their own of values.
enum TaskCategory : uint16_t {
  // Cannot run at the same time as another non-concurrent task.
  TASK_CATEGORY_NONCONCURRENT_FOREGROUND,
  // Can run concurrently with other tasks.
  TASK_CATEGORY_FOREGROUND,
  // Can only start running when there are no foreground tasks to run. May run
  // at background thread priority, which means that it may take time to
  // complete once it starts running.
  TASK_CATEGORY_BACKGROUND,
  // Can only start running when there are no foreground tasks to run. Cannot
  // run at background thread priority, which means that it takes a normal time
  // to complete once it starts running. This is useful for a task that acquires
  // resources that must not be held for a long time because they are shared
  // with foreground work (e.g. a lock under which heavy work is performed).
  TASK_CATEGORY_BACKGROUND_WITH_NORMAL_THREAD_PRIORITY,

  LAST_TASK_CATEGORY = TASK_CATEGORY_BACKGROUND_WITH_NORMAL_THREAD_PRIORITY
};

}  // namespace cc

#endif  // CC_RASTER_TASK_CATEGORY_H_
