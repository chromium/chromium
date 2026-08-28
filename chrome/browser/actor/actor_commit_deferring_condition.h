// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_ACTOR_COMMIT_DEFERRING_CONDITION_H_
#define CHROME_BROWSER_ACTOR_ACTOR_COMMIT_DEFERRING_CONDITION_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/types/pass_key.h"
#include "chrome/browser/actor/site_policy.h"
#include "components/actor/core/aggregated_journal.h"
#include "components/actor/core/task_id.h"
#include "content/public/browser/commit_deferring_condition.h"

namespace content {
class NavigationHandle;
}

namespace actor {

class ActorTask;
class ExecutionEngine;

// Defers committing a page activation (e.g. BackForwardCache restoration or
// Prerender activation) in tabs under actor control in order to evaluate
// Origin Gating policies before the page commits.
class ActorCommitDeferringCondition : public content::CommitDeferringCondition {
 public:
  static std::unique_ptr<content::CommitDeferringCondition> MaybeCreate(
      content::NavigationHandle& navigation_handle,
      content::CommitDeferringCondition::NavigationType navigation_type);

  ActorCommitDeferringCondition(base::PassKey<ActorCommitDeferringCondition>,
                                content::NavigationHandle& navigation_handle,
                                const ActorTask& task);

  ActorCommitDeferringCondition(const ActorCommitDeferringCondition&) = delete;
  ActorCommitDeferringCondition& operator=(
      const ActorCommitDeferringCondition&) = delete;

  ~ActorCommitDeferringCondition() override;

  // content::CommitDeferringCondition:
  Result WillCommitNavigation(base::OnceClosure resume) override;
  const char* TraceEventName() const override;

 private:
  void OnNavigationConfirmationDecision(
      base::OnceClosure resume,
      std::unique_ptr<AggregatedJournal::PendingAsyncEntry> journal_entry,
      MayActOnUrlBlockReason block_reason);

  TaskId task_id_;
  base::WeakPtr<ExecutionEngine> execution_engine_;

  base::WeakPtrFactory<ActorCommitDeferringCondition> weak_factory_{this};
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_ACTOR_COMMIT_DEFERRING_CONDITION_H_
