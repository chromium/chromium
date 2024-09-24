// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/raster/task.h"

#include <ostream>
#include <utility>

#include "base/check.h"
#include "base/notreached.h"
#include "base/trace_event/trace_id_helper.h"

namespace cc {

TaskState::TaskState() : value_(Value::kNew) {}

TaskState::~TaskState() {
  DCHECK(value_ != Value::kRunning)
      << "Running task should never get destroyed.";
  DCHECK(value_ == Value::kFinished || value_ == Value::kCanceled)
      << "Task, if scheduled, should get concluded either in FINISHED or "
         "CANCELED state.";
}

bool TaskState::IsNew() const {
  return value_ == Value::kNew;
}

bool TaskState::IsScheduled() const {
  return value_ == Value::kScheduled;
}

bool TaskState::IsRunning() const {
  return value_ == Value::kRunning;
}

bool TaskState::IsFinished() const {
  return value_ == Value::kFinished;
}

bool TaskState::IsCanceled() const {
  return value_ == Value::kCanceled;
}

void TaskState::Reset() {
  value_ = Value::kNew;
}

std::string TaskState::ToString() const {
  switch (value_) {
    case Value::kNew:
      return "NEW";
    case Value::kScheduled:
      return "SCHEDULED";
    case Value::kRunning:
      return "RUNNING";
    case Value::kFinished:
      return "FINISHED";
    case Value::kCanceled:
      return "CANCELED";
  }
  NOTREACHED();
}

void TaskState::DidSchedule() {
  DCHECK(value_ == Value::kNew)
      << "Task should be in NEW state to get scheduled.";
  value_ = Value::kScheduled;
}

void TaskState::DidStart() {
  DCHECK(value_ == Value::kScheduled)
      << "Task should be only in SCHEDULED state to start, that is it should "
         "not be started or finished.";
  value_ = Value::kRunning;
}

void TaskState::DidFinish() {
  DCHECK(value_ == Value::kRunning)
      << "Task should be running and not finished earlier.";
  value_ = Value::kFinished;
}

void TaskState::DidCancel() {
  DCHECK(value_ == Value::kNew || value_ == Value::kScheduled)
      << "Task should be either new or scheduled to get canceled.";
  value_ = Value::kCanceled;
}

Task::Task() = default;

Task::~Task() = default;

TaskGraph::TaskGraph() = default;

TaskGraph::TaskGraph(TaskGraph&& other) = default;

TaskGraph::~TaskGraph() = default;

TaskGraph::Node::Node(scoped_refptr<Task> new_task,
                      uint16_t category,
                      uint16_t priority,
                      uint32_t dependencies)
    : task(std::move(new_task)),
      category(category),
      priority(priority),
      dependencies(dependencies) {
  // Set a trace task id to use for connecting from where the task was posted.
  if (task) {
    task->set_trace_task_id(base::trace_event::GetNextGlobalTraceId());
  }
}

TaskGraph::Node::Node(Node&& other) = default;
TaskGraph::Node::~Node() = default;

void TaskGraph::Swap(TaskGraph* other) {
  nodes.swap(other->nodes);
  edges.swap(other->edges);
}

void TaskGraph::Reset() {
  nodes.clear();
  edges.clear();
}

}  // namespace cc
