// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/decorators/execution_context_priority_decorator.h"

#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/node_attached_data_impl.h"
#include "components/performance_manager/graph/worker_node_impl.h"
#include "components/performance_manager/public/execution_context/execution_context.h"

namespace performance_manager {

// Helper class providing access to the inline storage of the accepted vote in
// both the FrameNodeImpl class and the WorkerNodeImpl class.
class ExecutionContextPriorityAccess {
 public:
  static execution_context_priority::AcceptedVote* GetAcceptedVote(
      const execution_context::ExecutionContext* execution_context) {
    switch (execution_context->GetType()) {
      case execution_context::ExecutionContextType::kFrameNode:
        return &FrameNodeImpl::FromNode(execution_context->GetFrameNode())
                    ->accepted_vote_;
      case execution_context::ExecutionContextType::kWorkerNode:
        return &WorkerNodeImpl::FromNode(execution_context->GetWorkerNode())
                    ->accepted_vote_;
    }
  }
};

namespace execution_context_priority {

namespace {

// Sets the priority of an execution context.
void SetPriorityAndReason(
    const execution_context::ExecutionContext* execution_context,
    const PriorityAndReason& priority_and_reason) {
  switch (execution_context->GetType()) {
    case execution_context::ExecutionContextType::kFrameNode:
      FrameNodeImpl::FromNode(execution_context->GetFrameNode())
          ->SetPriorityAndReason(priority_and_reason);
      break;
    case execution_context::ExecutionContextType::kWorkerNode:
      WorkerNodeImpl::FromNode(execution_context->GetWorkerNode())
          ->SetPriorityAndReason(priority_and_reason);
      break;
  }
}

}  // namespace

ExecutionContextPriorityDecorator::ExecutionContextPriorityDecorator()
    : factory_(this) {}
ExecutionContextPriorityDecorator::~ExecutionContextPriorityDecorator() =
    default;

VotingChannel ExecutionContextPriorityDecorator::GetVotingChannel() {
  DCHECK_EQ(0u, factory_.voting_channels_issued());
  auto channel = factory_.BuildVotingChannel();
  voter_id_ = channel.voter_id();
  return channel;
}

VoteReceipt ExecutionContextPriorityDecorator::SubmitVote(
    util::PassKey<VotingChannel>,
    voting::VoterId<Vote> voter_id,
    const ExecutionContext* execution_context,
    const Vote& vote) {
  DCHECK_EQ(voter_id_, voter_id);
  auto* accepted_vote =
      ExecutionContextPriorityAccess::GetAcceptedVote(execution_context);
  DCHECK(!accepted_vote->IsValid());
  *accepted_vote = AcceptedVote(this, voter_id, execution_context, vote);
  SetPriorityAndReason(execution_context,
                       PriorityAndReason(vote.value(), vote.reason()));
  return accepted_vote->IssueReceipt();
}

void ExecutionContextPriorityDecorator::ChangeVote(util::PassKey<AcceptedVote>,
                                                   AcceptedVote* old_vote,
                                                   const Vote& new_vote) {
  const auto* execution_context = old_vote->context();
  auto* accepted_vote =
      ExecutionContextPriorityAccess::GetAcceptedVote(execution_context);
  DCHECK_EQ(accepted_vote, old_vote);
  DCHECK(accepted_vote->IsValid());
  accepted_vote->UpdateVote(new_vote);
  SetPriorityAndReason(execution_context,
                       PriorityAndReason(new_vote.value(), new_vote.reason()));
}

void ExecutionContextPriorityDecorator::VoteInvalidated(
    util::PassKey<AcceptedVote>,
    AcceptedVote* vote) {
  const auto* execution_context = vote->context();
  auto* accepted_vote =
      ExecutionContextPriorityAccess::GetAcceptedVote(execution_context);
  DCHECK_EQ(accepted_vote, vote);
  DCHECK(!accepted_vote->IsValid());
  // Update the priority by falling back to the default priority.
  SetPriorityAndReason(
      execution_context,
      PriorityAndReason(base::TaskPriority::LOWEST,
                        FrameNodeImpl::kDefaultPriorityReason));
}

}  // namespace execution_context_priority
}  // namespace performance_manager
