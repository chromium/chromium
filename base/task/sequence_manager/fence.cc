// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/sequence_manager/fence.h"

#include "base/check.h"
#include "base/json/values_util.h"
#include "base/task/sequence_manager/enqueue_order.h"
#include "base/task/sequence_manager/task_order.h"
#include "base/time/time.h"
#include "base/values.h"

namespace base {
namespace sequence_manager {
namespace internal {

Fence::Fence(const TaskOrder& task_order) : task_order_(task_order) {
  DCHECK_NE(task_order_.enqueue_order(), EnqueueOrder::none());
}

Fence::Fence(EnqueueOrder enqueue_order,
             TimeTicks delayed_run_time,
             int sequence_num)
    : task_order_(enqueue_order, delayed_run_time, sequence_num) {}

Fence::Fence(const Fence& other) = default;

Fence& Fence::operator=(const Fence& other) = default;

Fence::~Fence() = default;

// static
Fence Fence::BlockingFence() {
  return CreateWithEnqueueOrder(EnqueueOrder::blocking_fence());
}

// static
Fence Fence::CreateWithEnqueueOrder(EnqueueOrder enqueue_order) {
  return Fence(enqueue_order, TimeTicks(), 0);
}

}  // namespace internal
}  // namespace sequence_manager
}  // namespace base
