// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/decorators/frame_priority_decorator.h"

#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/node_attached_data_impl.h"

namespace performance_manager {

// Helper class providing access to FrameNodeImpl::accepted_vote_.
class FramePriorityAccess {
 public:
  static frame_priority::AcceptedVote* GetAcceptedVote(
      FrameNodeImpl* frame_node) {
    return &frame_node->accepted_vote_;
  }
};

namespace frame_priority {

FramePriorityDecorator::FramePriorityDecorator() : factory_(this) {}
FramePriorityDecorator::~FramePriorityDecorator() = default;

VotingChannel FramePriorityDecorator::GetVotingChannel() {
  DCHECK_EQ(0u, factory_.voting_channels_issued());
  auto channel = factory_.BuildVotingChannel();
  voter_id_ = channel.voter_id();
  return channel;
}

VoteReceipt FramePriorityDecorator::SubmitVote(VoterId voter_id,
                                               const Vote& vote) {
  DCHECK_EQ(voter_id_, voter_id);
  auto* frame_node = FrameNodeImpl::FromNode(vote.frame_node());
  auto* accepted_vote = FramePriorityAccess::GetAcceptedVote(frame_node);
  DCHECK(!accepted_vote->IsValid());
  *accepted_vote = AcceptedVote(this, voter_id, vote);
  frame_node->SetPriorityAndReason(
      PriorityAndReason(vote.priority(), vote.reason()));
  return accepted_vote->IssueReceipt();
}

VoteReceipt FramePriorityDecorator::ChangeVote(VoteReceipt receipt,
                                               AcceptedVote* old_vote,
                                               const Vote& new_vote) {
  auto* frame_node = FrameNodeImpl::FromNode(new_vote.frame_node());
  auto* accepted_vote = FramePriorityAccess::GetAcceptedVote(frame_node);
  DCHECK_EQ(accepted_vote, old_vote);
  DCHECK(accepted_vote->IsValid());
  accepted_vote->UpdateVote(new_vote);
  frame_node->SetPriorityAndReason(
      PriorityAndReason(new_vote.priority(), new_vote.reason()));
  return receipt;
}

void FramePriorityDecorator::VoteInvalidated(AcceptedVote* vote) {
  auto* frame_node = FrameNodeImpl::FromNode(vote->vote().frame_node());
  auto* accepted_vote = FramePriorityAccess::GetAcceptedVote(frame_node);
  DCHECK_EQ(accepted_vote, vote);
  DCHECK(!accepted_vote->IsValid());
  // Update the priority by falling back to the default priority.
  frame_node->SetPriorityAndReason(PriorityAndReason(
      base::TaskPriority::LOWEST, FrameNodeImpl::kDefaultPriorityReason));
}

}  // namespace frame_priority
}  // namespace performance_manager
