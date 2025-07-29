// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_navigation_throttle.h"

#include <algorithm>

#include "base/memory/ptr_util.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/execution_engine.h"
#include "chrome/browser/actor/site_policy.h"
#include "chrome/browser/profiles/profile.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle_registry.h"
#include "content/public/browser/web_contents.h"

namespace actor {

// static
void ActorNavigationThrottle::MaybeCreateAndAdd(
    content::NavigationThrottleRegistry& registry) {
  content::NavigationHandle& navigation_handle = registry.GetNavigationHandle();

  if (!navigation_handle.IsInPrimaryMainFrame() &&
      !navigation_handle.IsInPrerenderedMainFrame()) {
    return;
  }

  content::WebContents* web_contents = navigation_handle.GetWebContents();

  const auto* tab = tabs::TabInterface::MaybeGetFromContents(web_contents);
  if (!tab) {
    return;
  }
  const tabs::TabHandle tab_handle = tab->GetHandle();

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());

  auto* actor_service = actor::ActorKeyedService::Get(profile);
  if (!actor_service) {
    return;
  }

  const auto& tasks = actor_service->GetActiveTasks();
  auto task_it = std::ranges::find_if(tasks, [tab_handle](const auto& t) {
    const ActorTask* task = t.second;
    return task->IsActingOnTab(tab_handle);
  });
  if (task_it == tasks.end()) {
    return;
  }

  registry.AddThrottle(base::WrapUnique(
      new ActorNavigationThrottle(registry, *task_it->second)));
}

ActorNavigationThrottle::ActorNavigationThrottle(
    content::NavigationThrottleRegistry& registry,
    const ActorTask& task)
    : content::NavigationThrottle(registry),
      task_id_(task.id()),
      execution_engine_(task.GetExecutionEngine()->GetWeakPtr()) {}

ActorNavigationThrottle::~ActorNavigationThrottle() = default;

content::NavigationThrottle::ThrottleCheckResult
ActorNavigationThrottle::WillStartRequest() {
  return WillStartOrRedirectRequest(/*is_redirection=*/false);
}

content::NavigationThrottle::ThrottleCheckResult
ActorNavigationThrottle::WillRedirectRequest() {
  return WillStartOrRedirectRequest(/*is_redirection=*/true);
}

content::NavigationThrottle::ThrottleCheckResult
ActorNavigationThrottle::WillStartOrRedirectRequest(bool is_redirection) {
  const GURL& navigation_url = navigation_handle()->GetURL();
  const std::optional<url::Origin>& initiator_origin =
      navigation_handle()->GetInitiatorOrigin();

  AggregatedJournal& journal = GetJournal();

  if (!is_redirection && !initiator_origin) {
    journal.Log(navigation_url, task_id_, mojom::JournalTrack::kActor,
                "NavThrottle", "Proceed: not triggered by page");
    return content::NavigationThrottle::PROCEED;
  }

  if (initiator_origin && initiator_origin->IsSameOriginWith(navigation_url)) {
    journal.Log(navigation_url, task_id_, mojom::JournalTrack::kActor,
                "NavThrottle",
                is_redirection ? "Proceed: same origin redirect"
                               : "Proceed: same origin navigation");
    // This isn't needed for correctness. We know that if the actor triggered a
    // same origin navigation, the destination URL will be allowed. So we
    // avoid an unnecessary defer.
    return content::NavigationThrottle::PROCEED;
  }

  auto journal_entry = journal.CreatePendingAsyncEntry(
      navigation_url, task_id_, mojom::JournalTrack::kActor, "NavThrottle",
      is_redirection ? "Defer: check redirect safety"
                     : "Defer: check navigation safety");

  MayActOnUrl(
      navigation_url, /*allow_insecure_http=*/true, GetProfile(), journal,
      task_id_,
      base::BindOnce(&ActorNavigationThrottle::OnMayActOnUrlResult,
                     weak_factory_.GetWeakPtr(), std::move(journal_entry)));

  return content::NavigationThrottle::DEFER;
}

void ActorNavigationThrottle::OnMayActOnUrlResult(
    std::unique_ptr<AggregatedJournal::PendingAsyncEntry> journal_entry,
    bool may_act) {
  if (may_act) {
    journal_entry->EndEntry("Resume");
    Resume();
    return;
  }

  journal_entry->EndEntry("Cancel");
  // If the navigation we're about to cancel is attributable to the actor's tool
  // usage, consider the action a failure. But we don't consider canceled
  // prerenders to be an error.
  if (execution_engine_ && navigation_handle()->IsInPrimaryMainFrame()) {
    // As the effect of FailCurrentTool w.r.t. CancelDeferredNavigation is
    // asynchronous, order doesn't matter.
    execution_engine_->FailCurrentTool(
        mojom::ActionResultCode::kTriggeredNavigationBlocked);
  }
  // Regardless of whether the action is considered a failure, we cancel the
  // navigation itself.
  CancelDeferredNavigation(CANCEL_AND_IGNORE);
}

Profile* ActorNavigationThrottle::GetProfile() {
  return Profile::FromBrowserContext(
      navigation_handle()->GetWebContents()->GetBrowserContext());
}

AggregatedJournal& ActorNavigationThrottle::GetJournal() {
  return ActorKeyedService::Get(GetProfile())->GetJournal();
}

const char* ActorNavigationThrottle::GetNameForLogging() {
  return "ActorNavigationThrottle";
}

}  // namespace actor
