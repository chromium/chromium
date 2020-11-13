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
                                          public VoteObserver {
 public:
  ExecutionContextPriorityDecorator();
  ~ExecutionContextPriorityDecorator() override;

  // Issues a voting channel (registers the sole incoming voter).
  VotingChannel GetVotingChannel();

 protected:
  // VoteObserver implementation:
  void OnVoteSubmitted(VoterId voter_id,
                       const ExecutionContext* execution_context,
                       const Vote& vote) override;
  void OnVoteChanged(VoterId voter_id,
                     const ExecutionContext* context,
                     const Vote& new_vote) override;
  void OnVoteInvalidated(VoterId voter_id,
                         const ExecutionContext* context) override;

  // Provides the VotingChannel to our input voter.
  VoteConsumerDefaultImpl vote_consumer_default_impl_;

  // The ID of the only voting channel we've vended.
  voting::VoterId<Vote> voter_id_ = voting::kInvalidVoterId<Vote>;

 private:
  DISALLOW_COPY_AND_ASSIGN(ExecutionContextPriorityDecorator);
};

}  // namespace execution_context_priority
}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_DECORATORS_EXECUTION_CONTEXT_PRIORITY_DECORATOR_H_
