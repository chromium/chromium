// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/sequence_manager/test/fake_task.h"

namespace base {
namespace sequence_manager {

FakeTask::FakeTask() : FakeTask(0 /* task_type */) {}

FakeTask::FakeTask(TaskType task_type)
    : Task(internal::PostedTask(nullptr,
                                OnceClosure(),
                                FROM_HERE,
                                base::TimeDelta(),
                                Nestable::kNestable,
                                task_type),
           EnqueueOrder(),
           EnqueueOrder(),
           TimeTicks(),
           WakeUpResolution::kLow) {}

FakeTaskTiming::FakeTaskTiming() : TaskTiming(false /* has_wall_time */) {}

FakeTaskTiming::FakeTaskTiming(TimeTicks start, TimeTicks end)
    : FakeTaskTiming() {
  has_wall_time_ = true;
  start_time_ = start;
  end_time_ = end;
  state_ = State::Finished;
}

}  // namespace sequence_manager
}  // namespace base
