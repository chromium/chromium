// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/sequence_manager/task_order.h"

#include <functional>

#include "base/task/sequence_manager/enqueue_order.h"
#include "base/task/sequence_manager/tasks.h"

namespace base {
namespace sequence_manager {

namespace {

// Returns true iff `task_order1` Comparator{} `task_order2`. Used to
// implement other comparison operators.
template <typename Comparator>
static bool Compare(const base::sequence_manager::TaskOrder& task_order1,
                    const base::sequence_manager::TaskOrder& task_order2) {
  Comparator cmp{};

  if (task_order1.enqueue_order() != task_order2.enqueue_order())
    return cmp(task_order1.enqueue_order(), task_order2.enqueue_order());

  if (task_order1.delayed_run_time() != task_order2.delayed_run_time())
    return cmp(task_order1.delayed_run_time(), task_order2.delayed_run_time());

  // If the times happen to match, then we use the sequence number to decide.
  // Compare the difference to support integer roll-over.
  return cmp(task_order1.sequence_num() - task_order2.sequence_num(), 0);
}

}  // namespace

// Static
TaskOrder TaskOrder::CreateForTesting(EnqueueOrder enqueue_order,
                                      TimeTicks delayed_run_time,
                                      int sequence_num) {
  return TaskOrder(enqueue_order, delayed_run_time, sequence_num);
}

// Static
TaskOrder TaskOrder::CreateForTesting(EnqueueOrder enqueue_order) {
  return TaskOrder(enqueue_order, TimeTicks(), 0);
}

TaskOrder::TaskOrder(EnqueueOrder enqueue_order,
                     TimeTicks delayed_run_time,
                     int sequence_num)
    : enqueue_order_(enqueue_order),
      delayed_run_time_(delayed_run_time),
      sequence_num_(sequence_num) {}

TaskOrder::TaskOrder(const TaskOrder& other) = default;

TaskOrder& TaskOrder::operator=(const TaskOrder& other) = default;

TaskOrder::~TaskOrder() = default;

bool TaskOrder::operator>(const TaskOrder& other) const {
  return Compare<std::greater<>>(*this, other);
}

bool TaskOrder::operator<(const TaskOrder& other) const {
  return Compare<std::less<>>(*this, other);
}

bool TaskOrder::operator<=(const TaskOrder& other) const {
  return Compare<std::less_equal<>>(*this, other);
}

bool TaskOrder::operator>=(const TaskOrder& other) const {
  return Compare<std::greater_equal<>>(*this, other);
}

bool TaskOrder::operator==(const TaskOrder& other) const {
  return enqueue_order_ == other.enqueue_order_ &&
         delayed_run_time_ == other.delayed_run_time_ &&
         sequence_num_ == other.sequence_num_;
}

bool TaskOrder::operator!=(const TaskOrder& other) const {
  return !(*this == other);
}

}  // namespace sequence_manager
}  // namespace base
