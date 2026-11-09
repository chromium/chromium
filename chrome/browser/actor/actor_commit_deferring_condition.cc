// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_commit_deferring_condition.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/types/pass_key.h"
#include "build/build_config.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_navigation_throttle.h"
#include "chrome/browser/actor/actor_surface_handle.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/execution_engine.h"
#include "chrome/browser/actor/site_policy.h"
#include "chrome/browser/actor/tools/history_tool.h"
#include "chrome/common/chrome_features.h"
#include "components/actor/core/actor_features.h"
#include "components/actor/core/journal_details_builder.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/commit_deferring_condition.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/page_transition_types.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/device_info.h"
#endif

namespace actor {

namespace {

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

// Returns the delegate that confirms a user Back/Forward restored from the
// back/forward cache, or null if `navigation_handle` is not one or the
// confirmation is disabled on this platform.
base::WeakPtr<ActorNavigationThrottle::Delegate> GetUserBackForwardDelegate(
    content::NavigationHandle& navigation_handle,
    TaskId task_id) {
#if BUILDFLAG(IS_ANDROID)
  // On Android, only desktop form factors confirm user UI navigations.
  if (!base::android::device_info::is_desktop()) {
    return nullptr;
  }
#else
  if (!base::FeatureList::IsEnabled(features::kGlicConfirmTabClose)) {
    return nullptr;
  }
#endif
  if (!navigation_handle.IsServedFromBackForwardCache() ||
      navigation_handle.IsRendererInitiated() ||
      (navigation_handle.GetPageTransition() &
       ::ui::PAGE_TRANSITION_FORWARD_BACK) == 0 ||
      HistoryToolNavigation::GetForNavigationHandle(navigation_handle)) {
    return nullptr;
  }
  content::WebContents* web_contents = navigation_handle.GetWebContents();
  if (!web_contents) {
    return nullptr;
  }
  ActorKeyedService* service =
      ActorKeyedService::Get(web_contents->GetBrowserContext());
  ActorTask* task = service ? service->GetTask(task_id) : nullptr;
  if (!task) {
    return nullptr;
  }
  return task->navigation_delegate();
}

}  // namespace

// static
std::unique_ptr<content::CommitDeferringCondition>
ActorCommitDeferringCondition::MaybeCreate(
    content::NavigationHandle& navigation_handle,
    content::CommitDeferringCondition::NavigationType navigation_type) {
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

  const ActorSurfaceHandle actor_surface_handle =
      ActorSurfaceHandle::From(web_contents);
  if (actor_surface_handle.is_null()) {
    return nullptr;
  }

  auto* actor_service =
      actor::ActorKeyedService::Get(web_contents->GetBrowserContext());
  if (!actor_service) {
    return nullptr;
  }

  const ActorTask* task =
      actor_service->GetTaskFromActorSurface(actor_surface_handle);
  if (!task) {
    return nullptr;
  }

  // The user Back/Forward confirmation does not depend on origin gating.
  if (!base::FeatureList::IsEnabled(kGlicPageActivationGating) &&
      !GetUserBackForwardDelegate(navigation_handle, task->id())) {
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
  if (base::WeakPtr<ActorNavigationThrottle::Delegate> delegate =
          GetUserBackForwardDelegate(GetNavigationHandle(), task_id_)) {
    auto [resume_on_leave, resume_otherwise] =
        base::SplitOnceCallback(std::move(resume));
    // OnUserLeaveDialogDecision() ends this entry with the user's choice. If
    // the delegate shows no confirmation, it drops the callback and the entry
    // ends without details.
    std::unique_ptr<AggregatedJournal::PendingAsyncEntry> journal_entry =
        GetJournal().CreatePendingAsyncEntry(
            GetNavigationHandle().GetURL(), task_id_,
            MakeBrowserTrackUUID(task_id_), "ActorCommitDeferringCondition",
            JournalDetailsBuilder()
                .Add("defer", "Confirm user Back/Forward")
                .Build());
    if (delegate->MaybeDeferNavigation(
            tabs::TabInterface::MaybeGetFromContents(
                GetNavigationHandle().GetWebContents()),
            GetNavigationHandle().GetURL(),
            base::BindOnce(
                &ActorCommitDeferringCondition::OnUserLeaveDialogDecision,
                weak_factory_.GetWeakPtr(), std::move(resume_on_leave),
                std::move(journal_entry)))) {
      return Result::kDefer;
    }
    resume = std::move(resume_otherwise);
  }

  if (!base::FeatureList::IsEnabled(kGlicPageActivationGating)) {
    return Result::kProceed;
  }

  if (!execution_engine_) {
    return Result::kProceed;
  }

  std::unique_ptr<AggregatedJournal::PendingAsyncEntry> journal_entry =
      GetJournal().CreatePendingAsyncEntry(
          GetNavigationHandle().GetURL(), task_id_,
          MakeBrowserTrackUUID(task_id_), "ActorCommitDeferringCondition", {});

  execution_engine_->ShouldNavigationCommit(
      GetNavigationHandle(),
      base::BindOnce(
          &ActorCommitDeferringCondition::OnNavigationConfirmationDecision,
          weak_factory_.GetWeakPtr(), std::move(resume),
          std::move(journal_entry)));

  return Result::kDefer;
}

AggregatedJournal& ActorCommitDeferringCondition::GetJournal() {
  content::WebContents* web_contents = GetNavigationHandle().GetWebContents();
  CHECK(web_contents);
  auto* service = ActorKeyedService::Get(web_contents->GetBrowserContext());
  CHECK(service);
  return service->GetJournal();
}

const char* ActorCommitDeferringCondition::TraceEventName() const {
  return "ActorCommitDeferringCondition";
}

void ActorCommitDeferringCondition::OnNavigationConfirmationDecision(
    base::OnceClosure resume,
    std::unique_ptr<AggregatedJournal::PendingAsyncEntry> journal_entry,
    MayActOnUrlBlockReason block_reason) {
  CHECK(journal_entry);
  journal_entry->EndEntry(
      JournalDetailsBuilder().Add("block_reason", block_reason).Build());

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

void ActorCommitDeferringCondition::OnUserLeaveDialogDecision(
    base::OnceClosure resume,
    std::unique_ptr<AggregatedJournal::PendingAsyncEntry> journal_entry,
    bool may_continue) {
  CHECK(journal_entry);
  journal_entry->EndEntry(
      may_continue
          ? JournalDetailsBuilder()
                .Add("navigate", "User allowed navigation (Leaving task)")
                .Build()
          : JournalDetailsBuilder()
                .AddError("User cancelled navigation (Stayed)")
                .Build());
  content::WebContents* web_contents = GetNavigationHandle().GetWebContents();
  if (!web_contents) {
    return;
  }
  if (!may_continue) {
    // CommitDeferringCondition provides a resume callback but no
    // navigation-scoped cancellation callback. Use WebContents::Stop() to
    // cancel the deferred restore; this also aborts other loads in the tab.
    web_contents->Stop();
    return;
  }
  // StopTask() notifies observers, so do not touch members after it.
  if (auto* service =
          ActorKeyedService::Get(web_contents->GetBrowserContext())) {
    service->StopTask(task_id_, ActorTask::StoppedReason::kUserNavigatedAway);
  }
  std::move(resume).Run();
}

}  // namespace actor
