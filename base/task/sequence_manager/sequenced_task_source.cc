// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/sequence_manager/sequenced_task_source.h"

namespace base {
namespace sequence_manager {
namespace internal {

SequencedTaskSource::SelectedTask::SelectedTask(const SelectedTask&) = default;

SequencedTaskSource::SelectedTask::SelectedTask(
    Task& task,
    TaskExecutionTraceLogger task_execution_trace_logger,
    TaskQueue::QueuePriority priority,
    QueueName task_queue_name)
    : task(task),
      task_execution_trace_logger(task_execution_trace_logger),
      priority(priority),
      task_queue_name(task_queue_name) {}

SequencedTaskSource::SelectedTask::~SelectedTask() = default;

}  // namespace internal
}  // namespace sequence_manager
}  // namespace base
