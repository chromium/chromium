// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_navigation_throttle.h"

#include <algorithm>

#include "base/containers/fixed_flat_set.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/actor/actor_features.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_policy_checker.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/execution_engine.h"
#include "chrome/browser/actor/site_policy.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/actor/journal_details_builder.h"
#include "chrome/common/actor_webui.mojom.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle_registry.h"
#include "content/public/browser/web_contents.h"
#include "net/http/http_response_headers.h"

namespace actor {
namespace {
constexpr auto kBlockedMimeTypes = base::MakeFixedFlatSet<std::string_view>({
    "application/javascript",
    "application/json",
    "application/xml",
    "text/javascript",
    "text/csv",
    "text/json",
    "text/xml",
});
}  // namespace

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
ActorNavigationThrottle::WillProcessResponse() {
  if (base::FeatureList::IsEnabled(
          kGlicBlockNavigationToDangerousContentTypes)) {
    const net::HttpResponseHeaders* headers =
        navigation_handle()->GetResponseHeaders();
    std::string mime_type;
    if (headers->GetMimeType(&mime_type) &&
        kBlockedMimeTypes.contains(mime_type)) {
      GetJournal().Log(navigation_handle()->GetURL(), task_id_, "NavThrottle",
                       JournalDetailsBuilder()
                           .AddError("Navigate to disallowed content-type")
                           .Add("mime_type", mime_type)
                           .Build());

      // If the navigation we're about to cancel is attributable to the actor's
      // tool usage, consider the action a failure.
      if (navigation_handle()->IsInPrimaryMainFrame() && execution_engine_) {
        execution_engine_->FailCurrentTool(
            mojom::ActionResultCode::kTriggeredNavigationBlocked);
      }

      return content::NavigationThrottle::CANCEL_AND_IGNORE;
    }
  }

  if (!execution_engine_ ||
      !execution_engine_->ShouldGateNavigation(
          *navigation_handle(),
          base::BindOnce(
              &ActorNavigationThrottle::OnNavigationConfirmationDecision,
              weak_factory_.GetWeakPtr()))) {
    return content::NavigationThrottle::PROCEED;
  }
  // We do not invoke the callback which resumes/cancels the request
  // in pre-rendered frames.
  if (navigation_handle()->IsInPrerenderedMainFrame()) {
    return content::NavigationThrottle::CANCEL_AND_IGNORE;
  }
  return content::NavigationThrottle::DEFER;
}

void ActorNavigationThrottle::OnNavigationConfirmationDecision(
    bool may_continue) {
  CHECK(!navigation_handle()->IsInPrerenderedMainFrame())
      << "We should not be prompting for pre-rendered frame navigations.";
  if (may_continue) {
    Resume();
    return;
  }
  AggregatedJournal& journal = GetJournal();
  journal.Log(
      navigation_handle()->GetURL(), task_id_, "NavThrottle",
      JournalDetailsBuilder().AddError("Navigate cross origin").Build());
  // If the navigation we're about to cancel is attributable to the actor's
  // tool usage, consider the action a failure.
  if (navigation_handle()->IsInPrimaryMainFrame() && execution_engine_) {
    execution_engine_->FailCurrentTool(
        mojom::ActionResultCode::kTriggeredNavigationBlocked);
  }
  CancelDeferredNavigation(CANCEL_AND_IGNORE);
}

content::NavigationThrottle::ThrottleCheckResult
ActorNavigationThrottle::WillStartOrRedirectRequest(bool is_redirection) {
  const GURL& navigation_url = navigation_handle()->GetURL();
  const std::optional<url::Origin>& initiator_origin =
      navigation_handle()->GetInitiatorOrigin();

  AggregatedJournal& journal = GetJournal();

  if (!is_redirection && !initiator_origin) {
    journal.Log(navigation_url, task_id_, "NavThrottle",
                JournalDetailsBuilder()
                    .Add("navigate", "Not triggered by page")
                    .Build());
    return content::NavigationThrottle::PROCEED;
  }

  if (initiator_origin && initiator_origin->IsSameOriginWith(navigation_url)) {
    journal.Log(navigation_url, task_id_, "NavThrottle",
                JournalDetailsBuilder()
                    .Add("navigate", is_redirection ? "Same origin redirect"
                                                    : "Same origin navigation")
                    .Build());
    // This isn't needed for correctness. We know that if the actor triggered a
    // same origin navigation, the destination URL will be allowed. So we
    // avoid an unnecessary defer.
    return content::NavigationThrottle::PROCEED;
  }

  auto journal_entry = journal.CreatePendingAsyncEntry(
      navigation_url, task_id_, MakeBrowserTrackUUID(task_id_), "NavThrottle",
      JournalDetailsBuilder()
          .Add("defer", is_redirection ? "Check redirect safety"
                                       : "Check navigation safety")
          .Build());

  ActorKeyedService::Get(GetProfile())
      ->GetPolicyChecker()
      .MayActOnUrl(
          navigation_url, /*allow_insecure_http=*/true, GetProfile(), journal,
          task_id_,
          base::BindOnce(&ActorNavigationThrottle::OnMayActOnUrlResult,
                         weak_factory_.GetWeakPtr(), std::move(journal_entry)));

  return content::NavigationThrottle::DEFER;
}

void ActorNavigationThrottle::OnMayActOnUrlResult(
    std::unique_ptr<AggregatedJournal::PendingAsyncEntry> journal_entry,
    MayActOnUrlBlockReason block_reason) {
  if (block_reason == MayActOnUrlBlockReason::kAllowed) {
    journal_entry->EndEntry(
        JournalDetailsBuilder().Add("result", "Resume").Build());
    Resume();
    return;
  }

  journal_entry->EndEntry(JournalDetailsBuilder().AddError("Cancel").Build());
  // If the navigation we're about to cancel is attributable to the actor's tool
  // usage, consider the action a failure. But we don't consider canceled
  // prerenders to be an error.
  if (execution_engine_ && navigation_handle()->IsInPrimaryMainFrame()) {
    mojom::ActionResultCode tool_failure_code;

    switch (block_reason) {
      case MayActOnUrlBlockReason::kExternalProtocol:
        if (base::FeatureList::IsEnabled(
                kGlicExternalProtocolActionResultCode)) {
          tool_failure_code =
              mojom::ActionResultCode::kExternalProtocolNavigationBlocked;
          break;
        }
        [[fallthrough]];
      case MayActOnUrlBlockReason::kActuactionDisabled:
      case MayActOnUrlBlockReason::kIpAddress:
      case MayActOnUrlBlockReason::kLookalikeDomain:
      case MayActOnUrlBlockReason::kOptimizationGuideBlock:
      case MayActOnUrlBlockReason::kSafeBrowsing:
      case MayActOnUrlBlockReason::kTabIsErrorDocument:
      case MayActOnUrlBlockReason::kUrlNotInAllowlist:
      case MayActOnUrlBlockReason::kWrongScheme:
        tool_failure_code =
            mojom::ActionResultCode::kTriggeredNavigationBlocked;
        break;
      case MayActOnUrlBlockReason::kAllowed:
        NOTREACHED();
    }

    // As the effect of FailCurrentTool w.r.t. CancelDeferredNavigation is
    // asynchronous, order doesn't matter.
    execution_engine_->FailCurrentTool(tool_failure_code);
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
