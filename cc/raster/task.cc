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

TaskState::TaskState() : value_(Value::NEW) {}

TaskState::~TaskState() {
  DCHECK(value_ != Value::RUNNING)
      << "Running task should never get destroyed.";
  DCHECK(value_ == Value::FINISHED || value_ == Value::CANCELED)
      << "Task, if scheduled, should get concluded either in FINISHED or "
         "CANCELED state.";
}

bool TaskState::IsNew() const {
  return value_ == Value::NEW;
}

bool TaskState::IsScheduled() const {
  return value_ == Value::SCHEDULED;
}

bool TaskState::IsRunning() const {
  return value_ == Value::RUNNING;
}

bool TaskState::IsFinished() const {
  return value_ == Value::FINISHED;
}

bool TaskState::IsCanceled() const {
  return value_ == Value::CANCELED;
}

void TaskState::Reset() {
  value_ = Value::NEW;
}

std::string TaskState::ToString() const {
  switch (value_) {
    case Value::NEW:
      return "NEW";
    case Value::SCHEDULED:
      return "SCHEDULED";
    case Value::RUNNING:
      return "RUNNING";
    case Value::FINISHED:
      return "FINISHED";
    case Value::CANCELED:
      return "CANCELED";
  }
  NOTREACHED();
  return "";
}

void TaskState::DidSchedule() {
  DCHECK(value_ == Value::NEW)
      << "Task should be in NEW state to get scheduled.";
  value_ = Value::SCHEDULED;
}

void TaskState::DidStart() {
  DCHECK(value_ == Value::SCHEDULED)
      << "Task should be only in SCHEDULED state to start, that is it should "
         "not be started or finished.";
  value_ = Value::RUNNING;
}

void TaskState::DidFinish() {
  DCHECK(value_ == Value::RUNNING)
      << "Task should be running and not finished earlier.";
  value_ = Value::FINISHED;
}

void TaskState::DidCancel() {
  DCHECK(value_ == Value::NEW || value_ == Value::SCHEDULED)
      << "Task should be either new or scheduled to get canceled.";
  value_ = Value::CANCELED;
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
