// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_commit_deferring_condition.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/types/pass_key.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/execution_engine.h"
#include "chrome/browser/actor/site_policy.h"
#include "components/actor/core/actor_features.h"
#include "components/actor/core/journal_details_builder.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/commit_deferring_condition.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

namespace actor {

namespace {

AggregatedJournal* GetJournal(content::BrowserContext* context) {
  auto* service = ActorKeyedService::Get(context);
  if (!service) {
    return nullptr;
  }
  return &service->GetJournal();
}

bool IsPageActivation(
    const content::NavigationHandle& navigation_handle,
    content::CommitDeferringCondition::NavigationType navigation_type) {
  // `navigation_handle.IsPageActivation()` would check
  // `activating_prerender_host_id_` which has not yet been set, so we check the
  // navigation type instead.
  return navigation_type == content::CommitDeferringCondition::NavigationType::
                                kPrerenderedPageActivation ||
         navigation_handle.IsServedFromBackForwardCache();
}

}  // namespace

// static
std::unique_ptr<content::CommitDeferringCondition>
ActorCommitDeferringCondition::MaybeCreate(
    content::NavigationHandle& navigation_handle,
    content::CommitDeferringCondition::NavigationType navigation_type) {
  if (!base::FeatureList::IsEnabled(kGlicPageActivationGating)) {
    return nullptr;
  }

  if (!IsPageActivation(navigation_handle, navigation_type)) {
    return nullptr;
  }

  if (!navigation_handle.IsInPrimaryMainFrame()) {
    return nullptr;
  }

  content::WebContents* web_contents = navigation_handle.GetWebContents();
  if (!web_contents) {
    return nullptr;
  }

  const auto* tab = tabs::TabInterface::MaybeGetFromContents(web_contents);
  if (!tab) {
    return nullptr;
  }

  auto* actor_service =
      actor::ActorKeyedService::Get(web_contents->GetBrowserContext());
  if (!actor_service) {
    return nullptr;
  }

  const ActorTask* task = actor_service->GetTaskFromTab(*tab);
  if (!task) {
    return nullptr;
  }

  return std::make_unique<ActorCommitDeferringCondition>(
      base::PassKey<ActorCommitDeferringCondition>(), navigation_handle, *task);
}

ActorCommitDeferringCondition::ActorCommitDeferringCondition(
    base::PassKey<ActorCommitDeferringCondition>,
    content::NavigationHandle& navigation_handle,
    const ActorTask& task)
    : content::CommitDeferringCondition(navigation_handle),
      task_id_(task.id()),
      execution_engine_(task.GetExecutionEngine().GetWeakPtr()) {}

ActorCommitDeferringCondition::~ActorCommitDeferringCondition() = default;

content::CommitDeferringCondition::Result
ActorCommitDeferringCondition::WillCommitNavigation(base::OnceClosure resume) {
  if (!execution_engine_) {
    return Result::kProceed;
  }

  std::unique_ptr<AggregatedJournal::PendingAsyncEntry> journal_entry;
  if (AggregatedJournal* journal = GetJournal(
          GetNavigationHandle().GetWebContents()->GetBrowserContext())) {
    journal_entry = journal->CreatePendingAsyncEntry(
        GetNavigationHandle().GetURL(), task_id_,
        MakeBrowserTrackUUID(task_id_), "ActorCommitDeferringCondition", {});
  }

  execution_engine_->ShouldNavigationCommit(
      GetNavigationHandle(),
      base::BindOnce(
          &ActorCommitDeferringCondition::OnNavigationConfirmationDecision,
          weak_factory_.GetWeakPtr(), std::move(resume),
          std::move(journal_entry)));

  return Result::kDefer;
}

const char* ActorCommitDeferringCondition::TraceEventName() const {
  return "ActorCommitDeferringCondition";
}

void ActorCommitDeferringCondition::OnNavigationConfirmationDecision(
    base::OnceClosure resume,
    std::unique_ptr<AggregatedJournal::PendingAsyncEntry> journal_entry,
    MayActOnUrlBlockReason block_reason) {
  if (journal_entry) {
    journal_entry->EndEntry(
        JournalDetailsBuilder().Add("block_reason", block_reason).Build());
  }

  if (block_reason == MayActOnUrlBlockReason::kAllowed) {
    std::move(resume).Run();
    return;
  }

  if (execution_engine_) {
    execution_engine_->FailCurrentTool(
        BlockReasonToResultCode(block_reason, /*for_navigation=*/true));
  }

  if (auto* web_contents = GetNavigationHandle().GetWebContents()) {
    web_contents->Stop();
  }
}

}  // namespace actor
