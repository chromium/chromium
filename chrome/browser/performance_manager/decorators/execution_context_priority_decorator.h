// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_DECORATORS_EXECUTION_CONTEXT_PRIORITY_DECORATOR_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_DECORATORS_EXECUTION_CONTEXT_PRIORITY_DECORATOR_H_

#include "components/performance_manager/public/execution_context_priority/execution_context_priority.h"
#include "components/performance_manager/public/graph/graph.h"

namespace performance_manager {
namespace execution_context_priority {

// The ExecutionContextPriorityDecorator acts as the root node of a hierarchy of
// execution context priority voters. It is responsible for taking aggregated
// votes and applying them to the actual nodes in a graph.
class ExecutionContextPriorityDecorator : public GraphOwnedDefaultImpl,
                                          public VoteConsumer {
 public:
  ExecutionContextPriorityDecorator();
  ~ExecutionContextPriorityDecorator() override;

  // Issues a voting channel (registers the sole incoming voter).
  VotingChannel GetVotingChannel();

 protected:
  // VoteConsumer implementation:
  VoteReceipt SubmitVote(VoterId voter_id, const Vote& vote) override;
  VoteReceipt ChangeVote(VoteReceipt receipt,
                         AcceptedVote* old_vote,
                         const Vote& new_vote) override;
  void VoteInvalidated(AcceptedVote* vote) override;

  // Our VotingChannelFactory for providing VotingChannels to our input voters.
  VotingChannelFactory factory_;

  // The ID of the only voting channel we've vended.
  VoterId voter_id_ = kInvalidVoterId;

 private:
  DISALLOW_COPY_AND_ASSIGN(ExecutionContextPriorityDecorator);
};

}  // namespace execution_context_priority
}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_DECORATORS_EXECUTION_CONTEXT_PRIORITY_DECORATOR_H_
