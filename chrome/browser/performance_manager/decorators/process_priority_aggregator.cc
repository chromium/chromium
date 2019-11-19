// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/decorators/process_priority_aggregator.h"

#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/node_attached_data_impl.h"
#include "components/performance_manager/graph/process_node_impl.h"

namespace performance_manager {

class ProcessPriorityAggregatorAccess {
 public:
  using StorageType = decltype(ProcessNodeImpl::process_priority_data_);

  static std::unique_ptr<NodeAttachedData>* GetUniquePtrStorage(
      ProcessNodeImpl* process_node) {
    return &process_node->process_priority_data_;
  }
};

namespace {

class DataImpl : public ProcessPriorityAggregator::Data,
                 public NodeAttachedDataImpl<DataImpl> {
 public:
  using StorageType = ProcessPriorityAggregatorAccess::StorageType;

  struct Traits : public NodeAttachedDataOwnedByNodeType<ProcessNodeImpl> {};

  explicit DataImpl(const ProcessNode* process_node) {}
  ~DataImpl() override {}

  static std::unique_ptr<NodeAttachedData>* GetUniquePtrStorage(
      ProcessNodeImpl* process_node) {
    return ProcessPriorityAggregatorAccess::GetUniquePtrStorage(process_node);
  }
};

}  // namespace

void ProcessPriorityAggregator::Data::Decrement(base::TaskPriority priority) {
  switch (priority) {
    case base::TaskPriority::LOWEST:
#if DCHECK_IS_ON()
      DCHECK_LT(0u, lowest_count_);
      --lowest_count_;
#endif
      return;

    case base::TaskPriority::USER_VISIBLE: {
      DCHECK_LT(0u, user_visible_count_);
      --user_visible_count_;
      return;
    }

    case base::TaskPriority::USER_BLOCKING: {
      DCHECK_LT(0u, user_blocking_count_);
      --user_blocking_count_;
      return;
    }
  }

  NOTREACHED();
}

void ProcessPriorityAggregator::Data::Increment(base::TaskPriority priority) {
  switch (priority) {
    case base::TaskPriority::LOWEST:
#if DCHECK_IS_ON()
      ++lowest_count_;
#endif
      return;

    case base::TaskPriority::USER_VISIBLE: {
      ++user_visible_count_;
      return;
    }

    case base::TaskPriority::USER_BLOCKING: {
      ++user_blocking_count_;
      return;
    }
  }

  NOTREACHED();
}

bool ProcessPriorityAggregator::Data::IsEmpty() const {
#if DCHECK_IS_ON()
  if (lowest_count_)
    return false;
#endif
  return user_blocking_count_ == 0 && user_blocking_count_ == 0;
}

base::TaskPriority ProcessPriorityAggregator::Data::GetPriority() const {
  if (user_blocking_count_ > 0)
    return base::TaskPriority::USER_BLOCKING;
  if (user_visible_count_ > 0)
    return base::TaskPriority::USER_VISIBLE;
  return base::TaskPriority::LOWEST;
}

// static
ProcessPriorityAggregator::Data* ProcessPriorityAggregator::Data::GetForTesting(
    ProcessNodeImpl* process_node) {
  return DataImpl::Get(process_node);
}

ProcessPriorityAggregator::ProcessPriorityAggregator() = default;
ProcessPriorityAggregator::~ProcessPriorityAggregator() = default;

void ProcessPriorityAggregator::OnFrameNodeAdded(const FrameNode* frame_node) {
  auto* process_node = ProcessNodeImpl::FromNode(frame_node->GetProcessNode());
  DataImpl* data = DataImpl::Get(process_node);
  data->Increment(frame_node->GetPriorityAndReason().priority());
  // This is a nop if the priority didn't actually change.
  process_node->set_priority(data->GetPriority());
}

void ProcessPriorityAggregator::OnBeforeFrameNodeRemoved(
    const FrameNode* frame_node) {
  auto* process_node = ProcessNodeImpl::FromNode(frame_node->GetProcessNode());
  DataImpl* data = DataImpl::Get(process_node);
  data->Decrement(frame_node->GetPriorityAndReason().priority());
  // This is a nop if the priority didn't actually change.
  process_node->set_priority(data->GetPriority());
}

void ProcessPriorityAggregator::OnPriorityAndReasonChanged(
    const FrameNode* frame_node,
    const PriorityAndReason& previous_value) {
  // If the priority itself didn't change then ignore this notification.
  const PriorityAndReason& new_value = frame_node->GetPriorityAndReason();
  if (new_value.priority() == previous_value.priority())
    return;

  // Update the distinct frame priority counts, and set the process priority
  // accordingly.
  auto* process_node = ProcessNodeImpl::FromNode(frame_node->GetProcessNode());
  DataImpl* data = DataImpl::Get(process_node);
  data->Decrement(previous_value.priority());
  data->Increment(new_value.priority());
  // This is a nop if the priority didn't actually change.
  process_node->set_priority(data->GetPriority());
}

void ProcessPriorityAggregator::OnPassedToGraph(Graph* graph) {
  graph->AddFrameNodeObserver(this);
  graph->AddProcessNodeObserver(this);
}

void ProcessPriorityAggregator::OnTakenFromGraph(Graph* graph) {
  graph->RemoveProcessNodeObserver(this);
  graph->RemoveFrameNodeObserver(this);
}

void ProcessPriorityAggregator::OnProcessNodeAdded(
    const ProcessNode* process_node) {
  auto* process_node_impl = ProcessNodeImpl::FromNode(process_node);
  DCHECK(!DataImpl::Get(process_node_impl));
  DataImpl* data = DataImpl::GetOrCreate(process_node_impl);
  DCHECK(data->IsEmpty());
  DCHECK_EQ(base::TaskPriority::LOWEST, process_node_impl->priority());
  DCHECK_EQ(base::TaskPriority::LOWEST, data->GetPriority());
}

void ProcessPriorityAggregator::OnBeforeProcessNodeRemoved(
    const ProcessNode* process_node) {
#if DCHECK_IS_ON()
  auto* process_node_impl = ProcessNodeImpl::FromNode(process_node);
  DataImpl* data = DataImpl::Get(process_node_impl);
  DCHECK(data->IsEmpty());
#endif
}

}  // namespace performance_manager
