// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/execution_engine.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <utility>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/check_is_test.h"
#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/notimplemented.h"
#include "base/state_transitions.h"
#include "base/trace_event/trace_event.h"
#include "base/types/cxx23_to_underlying.h"
#include "base/types/id_type.h"
#include "chrome/browser/actor/actor_features.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_metrics.h"
#include "chrome/browser/actor/actor_policy_checker.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/actor_util.h"
#include "chrome/browser/actor/browser_action_util.h"
#include "chrome/browser/actor/safety_list_manager.h"
#include "chrome/browser/actor/site_policy.h"
#include "chrome/browser/actor/tools/navigate_tool_request.h"
#include "chrome/browser/actor/tools/tool_controller.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/browser/actor/ui/event_dispatcher.h"
#include "chrome/browser/autofill/glic/actor_form_filling_service_impl.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/password_manager/actor_login/actor_login_service.h"
#include "chrome/browser/password_manager/actor_login/actor_login_service_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/actor/journal_details_builder.h"
#include "chrome/common/actor/task_id.h"
#include "chrome/common/chrome_features.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/base/proto_wrapper.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"
#include "ui/event_dispatcher.h"
#include "url/origin.h"

using content::RenderFrameHost;
using content::WebContents;
using optimization_guide::DocumentIdentifierUserData;
using optimization_guide::proto::Action;
using optimization_guide::proto::Actions;
using optimization_guide::proto::ActionTarget;
using optimization_guide::proto::AnnotatedPageContent;
using tabs::TabInterface;

namespace actor {

namespace {

const RenderFrameHost* GetPrimaryMainFrame(
    content::NavigationHandle& navigation_handle) {
  return navigation_handle.GetWebContents()->GetPrimaryMainFrame();
}

void PostTaskForActCallback(
    ActorTask::ActCallback callback,
    mojom::ActionResultPtr result,
    std::optional<size_t> index_of_failed_action,
    std::vector<ActionResultWithLatencyInfo> action_results) {
  // Using a sparse histogram instead of a linear (i.e. enumeration) histogram
  // here because, the linear histograms are limited to 1000 values in
  // base/metrics/histogram.cc.
  base::UmaHistogramSparse("Actor.ExecutionEngine.Action.ResultCode",
                           base::to_underlying(result->code));
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), std::move(result),
                     index_of_failed_action, std::move(action_results)));
}

// Helper to determine if we should gate and thus send an IPC when navigtating
// to a new origin. See note on `kGlicNavigationGatingUseSiteNotOrigin`.
bool IsSameForNewOriginNavigationGating(const url::Origin& reference_origin,
                                        const GURL& navigation_url) {
  CHECK(IsNavigationGatingEnabled());

  if (kGlicNavigationGatingUseSiteNotOrigin.Get()) {
    return net::SchemefulSite::IsSameSite(reference_origin.GetURL(),
                                          navigation_url);
  }

  return reference_origin.IsSameOriginWith(navigation_url);
}

}  // namespace

ToolDelegate::CredentialWithPermission::CredentialWithPermission() = default;
ToolDelegate::CredentialWithPermission::CredentialWithPermission(
    const actor_login::Credential& credential,
    webui::mojom::UserGrantedPermissionDuration permission_duration)
    : credential(credential), permission_duration(permission_duration) {}
ToolDelegate::CredentialWithPermission::CredentialWithPermission(
    const CredentialWithPermission&) = default;
ToolDelegate::CredentialWithPermission::CredentialWithPermission(
    CredentialWithPermission&&) = default;
ToolDelegate::CredentialWithPermission&
ToolDelegate::CredentialWithPermission::operator=(
    const CredentialWithPermission&) = default;
ToolDelegate::CredentialWithPermission&
ToolDelegate::CredentialWithPermission::operator=(CredentialWithPermission&&) =
    default;
ToolDelegate::CredentialWithPermission::~CredentialWithPermission() = default;

ExecutionEngine::ExecutionEngine(Profile* profile)
    : profile_(profile),
      journal_(ActorKeyedService::Get(profile)->GetJournal().GetSafeRef()),
      ui_event_dispatcher_(ui::NewUiEventDispatcher(
          ActorKeyedService::Get(profile)->GetActorUiStateManager())) {
  TRACE_EVENT0("actor", "ExecutionEngine::ExecutionEngine");
  CHECK(profile_);
}

ExecutionEngine::ExecutionEngine(
    Profile* profile,
    std::unique_ptr<ui::UiEventDispatcher> ui_event_dispatcher)
    : profile_(profile),
      journal_(ActorKeyedService::Get(profile)->GetJournal().GetSafeRef()),
      ui_event_dispatcher_(std::move(ui_event_dispatcher)) {
  TRACE_EVENT0("actor", "ExecutionEngine::ExecutionEngine");
  CHECK(profile_);
}

std::unique_ptr<ExecutionEngine> ExecutionEngine::CreateForTesting(
    Profile* profile,
    std::unique_ptr<ui::UiEventDispatcher> ui_event_dispatcher) {
  return base::WrapUnique<ExecutionEngine>(
      new ExecutionEngine(profile, std::move(ui_event_dispatcher)));
}

ExecutionEngine::~ExecutionEngine() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::UmaHistogramCounts1000("Actor.NavigationGating.AllowListSize",
                               allowed_navigation_origins_.size());
  base::UmaHistogramCounts1000("Actor.NavigationGating.ConfirmedListSize",
                               user_confirmed_blocklisted_origins_.size());

  RunUserTakeoverCallbackIfExists(/*should_cancel=*/true);
}

void ExecutionEngine::SetOwner(ActorTask* task) {
  task_ = task;
  TRACE_EVENT0("actor", "ExecutionEngine::SetOwner");
  actor_login_service_ = std::make_unique<actor_login::ActorLoginServiceImpl>();
  actor_form_filling_service_ =
      std::make_unique<autofill::ActorFormFillingServiceImpl>();
  tool_controller_ = std::make_unique<ToolController>(*task_, *this);
}

void ExecutionEngine::SetState(State state) {
  TRACE_EVENT0("actor", "ExecutionEngine::SetState");
  journal_->Log(GURL(), task_->id(), "ExecutionEngine::StateChange",
                JournalDetailsBuilder()
                    .Add("current_state", StateToString(state_))
                    .Add("new_state", StateToString(state))
                    .Build());

#if DCHECK_IS_ON()
  static const base::NoDestructor<base::StateTransitions<State>> transitions(
      base::StateTransitions<State>({
          {State::kInit, {State::kStartAction, State::kComplete}},
          {State::kStartAction,
           {State::kToolCreateAndVerify, State::kComplete}},
          {State::kToolCreateAndVerify,
           {State::kUiPreInvoke, State::kComplete}},
          {State::kUiPreInvoke, {State::kToolInvoke, State::kComplete}},
          {State::kToolInvoke, {State::kUiPostInvoke, State::kComplete}},
          {State::kUiPostInvoke, {State::kComplete, State::kStartAction}},
          {State::kComplete, {State::kStartAction}},
      }));
  DCHECK_STATE_TRANSITION(transitions, state_, state);
#endif  // DCHECK_IS_ON()
  observers_.Notify(&StateObserver::OnStateChanged, state_, state);
  state_ = state;
}

std::string ExecutionEngine::StateToString(State state) {
  switch (state) {
    case State::kInit:
      return "INIT";
    case State::kStartAction:
      return "START_ACTION";
    case State::kToolCreateAndVerify:
      return "CREATE_AND_VERIFY";
    case State::kUiPreInvoke:
      return "UI_PRE_INVOKE";
    case State::kToolInvoke:
      return "TOOL_INVOKE";
    case State::kUiPostInvoke:
      return "UI_POST_INVOKE";
    case State::kComplete:
      return "COMPLETE";
  }
}

bool ExecutionEngine::ShouldGateNavigation(
    content::NavigationHandle& navigation_handle,
    ExecutionEngine::NavigationDecisionCallback callback) {
  if (!IsNavigationGatingEnabled()) {
    return false;
  }

  CHECK(navigation_handle.GetNavigatingFrameType() ==
            content::FrameType::kPrimaryMainFrame ||
        navigation_handle.GetNavigatingFrameType() ==
            content::FrameType::kPrerenderMainFrame);

  GatingDecision decision =
      ShouldGateNavigationInternal(navigation_handle, std::move(callback));
  if (decision == GatingDecision::kNeedsAsyncCheck) {
    return true;
  }

  bool applied_gate = decision == GatingDecision::kBlockByStaticList;
  LogNavigationGating(
      /*initiator_origin=*/GetPrimaryMainFrame(navigation_handle)
          ->GetLastCommittedOrigin(),
      navigation_handle.GetURL(), applied_gate);
  return applied_gate;
}

ExecutionEngine::GatingDecision ExecutionEngine::ShouldGateNavigationInternal(
    content::NavigationHandle& navigation_handle,
    ExecutionEngine::NavigationDecisionCallback callback) {
  CHECK(!navigation_handle.HasCommitted());
  base::ScopedUmaHistogramTimer timer(
      "Actor.NavigationGating.TimeElapsedForGating");

  const GURL source_url =
      GetPrimaryMainFrame(navigation_handle)->GetLastCommittedURL();
  const GURL& destination_url = navigation_handle.GetURL();
  const GatingDecision decision =
      DetermineGatingDecision(source_url, destination_url);
  base::UmaHistogramEnumeration("Actor.NavigationGating.GatingDecision",
                                decision);
  if (decision == GatingDecision::kBlockByStaticList) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), /*may_continue=*/false));
  } else if (decision == GatingDecision::kNeedsAsyncCheck) {
    bool skip_prompt = navigation_handle.IsInPrerenderedMainFrame();
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&ExecutionEngine::CheckNavigationBlocklist, GetWeakPtr(),
                       navigation_handle.GetInitiatorOrigin(), destination_url,
                       skip_prompt, std::move(callback)));
  }

  return decision;
}

void ExecutionEngine::LogNavigationGating(
    const std::optional<url::Origin>& initiator_origin,
    const GURL& navigation_url,
    bool applied_gate) {
  UMA_HISTOGRAM_BOOLEAN("Actor.NavigationGating.AppliedGate", applied_gate);

  if (initiator_origin) {
    UMA_HISTOGRAM_BOOLEAN("Actor.NavigationGating.CrossOrigin",
                          !initiator_origin->IsSameOriginWith(
                              url::Origin::Create(navigation_url)));
    UMA_HISTOGRAM_BOOLEAN("Actor.NavigationGating.CrossSite",
                          !net::SchemefulSite::IsSameSite(
                              initiator_origin->GetURL(), navigation_url));
  }
}

ExecutionEngine::GatingDecision ExecutionEngine::DetermineGatingDecision(
    const GURL& source_url,
    const GURL& destination_url) const {
  url::Origin destination_origin = url::Origin::Create(destination_url);
  const SafetyListManager& safety_list_manager =
      *SafetyListManager::GetInstance();

  if (url::IsSameOriginWith(source_url, destination_url)) {
    // The static blocklist can still block same-origin navigations. A wildcard
    // source entry like `[*, foo.com]` will block a `foo.com -> foo.com`
    // navigation. The reasoning is that a URL globally blocked as a
    // destination should not be reachable from anywhere, including itself.
    // Conversely, a source-specific entry like `[foo.com, *]` will *not* block
    // a `foo.com -> foo.com` navigation. This is because a global block on
    // navigations *from* a URL is intended to prevent leaving that origin, not
    // moving within it.
    return safety_list_manager.get_blocked_list()
                   .ContainsUrlPairWithWildcardSource(source_url,
                                                      destination_url)
               ? GatingDecision::kBlockByStaticList
               : GatingDecision::kAllowSameOrigin;
  }

  if (safety_list_manager.get_blocked_list().ContainsUrlPair(source_url,
                                                             destination_url)) {
    return GatingDecision::kBlockByStaticList;
  }

  if (safety_list_manager.get_allowed_list().ContainsUrlPair(source_url,
                                                             destination_url)) {
    return GatingDecision::kAllowByStaticList;
  }

  return GatingDecision::kNeedsAsyncCheck;
}

void ExecutionEngine::CheckNavigationBlocklist(
    const std::optional<url::Origin>& initiator_origin,
    const GURL& navigation_url,
    bool skip_prompt,
    ExecutionEngine::NavigationDecisionCallback callback) {
  // Check previously confirmed origins on the sensitive blocklist. If the user
  // has previously confirmed the origin is allowed, we should proceed and not
  // double prompt.
  for (const auto& origin : user_confirmed_blocklisted_origins_) {
    if (origin.IsSameOriginWith(navigation_url)) {
      OnNavigationBlocklistDecision(initiator_origin, navigation_url,
                                    skip_prompt, std::move(callback),
                                    /*not_on_blocklist=*/true);
      return;
    }
  }
  auto [callback1, callback2] = base::SplitOnceCallback(std::move(callback));
  if (ShouldBlockNavigationUrlForOriginGating(
          navigation_url, profile_,
          base::BindOnce(&ExecutionEngine::OnNavigationBlocklistDecision,
                         GetWeakPtr(), initiator_origin, navigation_url,
                         skip_prompt, std::move(callback1)))) {
    return;
  }
  // If `ShouldBlockNavigationUrlForOriginGating` returns false, it means the
  // Optimization Guide was not available to check the blocklist, so we
  // continue to the next step.
  OnNavigationBlocklistDecision(initiator_origin, navigation_url, skip_prompt,
                                std::move(callback2),
                                /*not_on_blocklist=*/true);
}

void ExecutionEngine::OnNavigationBlocklistDecision(
    const std::optional<url::Origin> initiator_origin,
    const GURL navigation_url,
    bool skip_prompt,
    ExecutionEngine::NavigationDecisionCallback callback,
    bool not_on_blocklist) {
  // If not blocked by blocklist, check if it's in origin the actor has
  // previously interacted with or received instructions from the server to
  // interact with.
  if (not_on_blocklist) {
    if (initiator_origin && IsSameForNewOriginNavigationGating(
                                initiator_origin.value(), navigation_url)) {
      LogNavigationGating(initiator_origin, navigation_url,
                          /*applied_gate=*/false);
      std::move(callback).Run(/*may_continue=*/true);
      return;
    }

    for (const auto& origin : allowed_navigation_origins_) {
      if (IsSameForNewOriginNavigationGating(origin, navigation_url)) {
        LogNavigationGating(initiator_origin, navigation_url,
                            /*applied_gate=*/false);
        std::move(callback).Run(/*may_continue=*/true);
        return;
      }
    }
  }

  // At this point, the navigation is either blocked OR not on the allowlist.
  LogNavigationGating(initiator_origin, navigation_url, /*applied_gate=*/true);

  if (skip_prompt) {
    std::move(callback).Run(/*may_continue=*/false);
    return;
  }

  // If the site is not on the blocklist, this is a novel origin and we should
  // either confirm the navigation with the web client or prompt the user
  // depending on the feature state.
  if (not_on_blocklist) {
    HandleNavigationToNewOrigin(url::Origin::Create(navigation_url),
                                std::move(callback));
    return;
  }

  // We use `kGlicPromptUserForSensitiveNavigations` to toggle user
  // confirmations when navigationg to a URL on the optimization guide
  // blocklist.
  if (!kGlicPromptUserForSensitiveNavigations.Get()) {
    std::move(callback).Run(/*may_continue=*/false);
    return;
  }

  // Otherwise if the site is blocked, present a user confirmation dialog to
  // continue.
  SendUserConfirmationDialogRequest(url::Origin::Create(navigation_url),
                                    /*for_blocklisted_origin=*/true,
                                    std::move(callback));
}

void ExecutionEngine::HandleNavigationToNewOrigin(
    const url::Origin& navigation_origin,
    ExecutionEngine::NavigationDecisionCallback callback) {
  if (!kGlicConfirmNavigationToNewOrigins.Get()) {
    std::move(callback).Run(/*may_continue=*/true);
    return;
  }
  if (kGlicPromptUserForNavigationToNewOrigins.Get()) {
    SendUserConfirmationDialogRequest(navigation_origin,
                                      /*for_blocklisted_origin=*/false,
                                      std::move(callback));
    return;
  }
  SendNavigationConfirmationRequest(navigation_origin, std::move(callback));
}

void ExecutionEngine::SendNavigationConfirmationRequest(
    const url::Origin& navigation_origin,
    ExecutionEngine::NavigationDecisionCallback callback) {
  if (!task_->delegate()) {
    std::move(callback).Run(/*may_continue=*/false);
    return;
  }
  task_->delegate()->RequestToConfirmNavigation(
      task_->id(), navigation_origin,
      base::BindOnce(&ExecutionEngine::OnNavigationConfirmationDecision,
                     GetWeakPtr(), navigation_origin, std::move(callback)));
}

void ExecutionEngine::OnNavigationConfirmationDecision(
    url::Origin navigation_origin,
    ExecutionEngine::NavigationDecisionCallback callback,
    webui::mojom::NavigationConfirmationResponsePtr response) {
  if (response->result->is_permission_granted()) {
    bool permission_granted = response->result->get_permission_granted();
    // TODO(dylancutler): Separate Actor.NavigationGating.PermissionGranted into
    // separate histograms for different confirmation types.
    UMA_HISTOGRAM_BOOLEAN("Actor.NavigationGating.PermissionGranted",
                          permission_granted);
    if (permission_granted) {
      allowed_navigation_origins_.insert(std::move(navigation_origin));
    }
    std::move(callback).Run(permission_granted);
    return;
  }
  // TODO(crbug.com/450302860): Add UMA metrics for logging frequency of
  // different failure modes.
  std::move(callback).Run(/*may_continue=*/false);
}

void ExecutionEngine::SendUserConfirmationDialogRequest(
    const url::Origin& navigation_origin,
    bool for_blocklisted_origin,
    ExecutionEngine::NavigationDecisionCallback callback) {
  if (!task_->delegate()) {
    std::move(callback).Run(/*may_continue=*/false);
    return;
  }
  task_->delegate()->RequestToShowUserConfirmationDialog(
      task_->id(), navigation_origin, for_blocklisted_origin,
      base::BindOnce(&ExecutionEngine::OnPromptUserToConfirmNavigationDecision,
                     GetWeakPtr(), navigation_origin, for_blocklisted_origin,
                     std::move(callback)));
}

void ExecutionEngine::OnPromptUserToConfirmNavigationDecision(
    url::Origin navigation_origin,
    bool for_blocklisted_origin,
    ExecutionEngine::NavigationDecisionCallback callback,
    webui::mojom::UserConfirmationDialogResponsePtr response) {
  if (response->result->is_permission_granted()) {
    bool permission_granted = response->result->get_permission_granted();
    UMA_HISTOGRAM_BOOLEAN("Actor.NavigationGating.PermissionGranted",
                          permission_granted);
    if (permission_granted) {
      allowed_navigation_origins_.insert(url::Origin(navigation_origin));
      // We update both lists in the `for_blocklisted_origin` case so that we do
      // not have to double-confirm this origin when we invoke
      // ExecutionEngine::HandleNavigationToNewOrigin.
      if (for_blocklisted_origin) {
        user_confirmed_blocklisted_origins_.insert(
            url::Origin(navigation_origin));
      }
    }
    std::move(callback).Run(permission_granted);
    return;
  }
  // TODO(crbug.com/450302860): Add UMA metrics for logging frequency of
  // different failure modes.
  std::move(callback).Run(/*may_continue=*/false);
}

void ExecutionEngine::UserTakeover(
    mojom::ActionResultCode takeover_response_code,
    base::OnceCallback<void(bool)> callback) {
  if (takeover_response_code == mojom::ActionResultCode::kFilePickerTriggered) {
    base::UmaHistogramBoolean("Actor.Download.SaveAsDialogTriggered", true);
  }

  CancelOngoingActions(takeover_response_code);

  // Cancel any existing user takeover callback
  RunUserTakeoverCallbackIfExists(/*should_cancel=*/true);

  user_takeover_callback_ = std::move(callback);
}

void ExecutionEngine::RunUserTakeoverCallbackIfExists(bool should_cancel) {
  if (user_takeover_callback_.is_null()) {
    return;
  }

  std::move(user_takeover_callback_).Run(should_cancel);
}

void ExecutionEngine::AddObserver(StateObserver* observer) {
  observers_.AddObserver(observer);
}

void ExecutionEngine::RemoveObserver(StateObserver* observer) {
  observers_.RemoveObserver(observer);
}

void ExecutionEngine::CancelOngoingActions(mojom::ActionResultCode reason) {
  TRACE_EVENT0("actor", "ExecutionEngine::CancelOngoingActions");
  if (tool_controller_) {
    tool_controller_->Cancel();
  }
  if (!action_sequence_.empty()) {
    CompleteActions(MakeResult(reason), /*action_index=*/std::nullopt);
  }
}

void ExecutionEngine::FailCurrentTool(mojom::ActionResultCode reason) {
  TRACE_EVENT0("actor", "ExecutionEngine::FailCurrentTool");
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_NE(reason, mojom::ActionResultCode::kOk);
  if (state_ != State::kToolInvoke) {
    return;
  }

  external_tool_failure_reason_ = reason;
}

void ExecutionEngine::Act(std::vector<std::unique_ptr<ToolRequest>>&& actions,
                          ActorTask::ActCallback callback) {
  TRACE_EVENT0("actor", "ExecutionEngine::Act");
  CHECK(base::FeatureList::IsEnabled(features::kGlicActor));
  CHECK(!actions.empty());
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(task_->GetState(), ActorTask::State::kActing);

  {
    JournalDetailsBuilder journal_details;
    for (size_t i = 0; i < actions.size(); ++i) {
      journal_details.Add(absl::StrFormat("Actions[%d]", i),
                          actions[i]->JournalEvent());
    }
    journal_->Log(GURL::EmptyGURL(), task_->id(), "ExecutionEngine::Act",
                  std::move(journal_details).Build());
  }

  if (!action_sequence_.empty()) {
    journal_->Log(
        actions[0]->GetURLForJournal(), task_->id(), "Act Failed",
        JournalDetailsBuilder()
            .AddError(
                "Unable to perform action: task already has action in progress")
            .Build());
    PostTaskForActCallback(
        std::move(callback),
        MakeResult(mojom::ActionResultCode::kExecutionEngineExistingAction),
        std::nullopt, {});
    return;
  }

  act_callback_ = std::move(callback);
  next_action_index_ = 0;

  absl::flat_hash_set<int32_t> acting_tab_handles;

  action_sequence_ = std::move(actions);
  for (const std::unique_ptr<ToolRequest>& action : action_sequence_) {
    CHECK(action);
    if (action->GetTabHandle() != tabs::TabHandle::Null()) {
      acting_tab_handles.insert(action->GetTabHandle().raw_value());
    }
    if (IsNavigationGatingEnabled()) {
      if (std::optional<url::Origin> maybe_origin =
              action->AssociatedOriginGrant();
          maybe_origin) {
        allowed_navigation_origins_.insert(maybe_origin.value());
      }
    }
  }

  KickOffNextAction();
}

void ExecutionEngine::KickOffNextAction() {
  TRACE_EVENT0("actor", "ExecutionEngine::KickOffNextAction");
  DCHECK(state_ == State::kInit || state_ == State::kUiPostInvoke ||
         state_ == State::kComplete)
      << "Current state is " << StateToString(state_);
  CHECK_LT(next_action_index_, action_sequence_.size());

  SetState(State::kStartAction);
  action_start_time_ = base::TimeTicks::Now();

  if (GetNextAction().RequiresUrlCheckInCurrentTab()) {
    SafetyChecksForNextAction();
  } else {
    ExecuteNextAction();
  }
}

void ExecutionEngine::SafetyChecksForNextAction() {
  TRACE_EVENT0("actor", "ExecutionEngine::SafetyChecksForNextAction");
  tabs::TabInterface* tab = GetNextAction().GetTabHandle().Get();

  if (!tab) {
    journal_->Log(GURL::EmptyGURL(), task_->id(), "Act Failed",
                  JournalDetailsBuilder()
                      .AddError("The tab is no longer present")
                      .Build());
    CompleteActions(MakeResult(mojom::ActionResultCode::kTabWentAway,
                               /*requires_page_stabilization=*/false,
                               "The tab is no longer present."),
                    next_action_index_);
    return;
  }

  // Asynchronously check if we can act on the tab.
  ActorKeyedService::Get(profile_)->GetPolicyChecker().MayActOnTab(
      *tab, *journal_, task_->id(), allowed_navigation_origins_,
      base::BindOnce(
          &ExecutionEngine::OnMayActOnTabDecision, GetWeakPtr(),
          tab->GetContents()->GetPrimaryMainFrame()->GetLastCommittedOrigin()));
}

void ExecutionEngine::OnMayActOnTabDecision(
    const url::Origin& evaluated_origin,
    MayActOnUrlBlockReason block_reason) {
  switch (block_reason) {
    case MayActOnUrlBlockReason::kAllowed:
      DidFinishAsyncSafetyChecks(evaluated_origin, /*may_act=*/true);
      return;
    case MayActOnUrlBlockReason::kOptimizationGuideBlock:
      if (IsNavigationGatingEnabled() &&
          kGlicPromptUserForSensitiveNavigations.Get()) {
        SendUserConfirmationDialogRequest(
            evaluated_origin,
            /*for_blocklisted_origin=*/true,
            base::BindOnce(&ExecutionEngine::DidFinishAsyncSafetyChecks,
                           GetWeakPtr(), evaluated_origin));
        return;
      }
      [[fallthrough]];
    case MayActOnUrlBlockReason::kActuactionDisabled:
    case MayActOnUrlBlockReason::kExternalProtocol:
    case MayActOnUrlBlockReason::kIpAddress:
    case MayActOnUrlBlockReason::kLookalikeDomain:
    case MayActOnUrlBlockReason::kSafeBrowsing:
    case MayActOnUrlBlockReason::kTabIsErrorDocument:
    case MayActOnUrlBlockReason::kUrlNotInAllowlist:
    case MayActOnUrlBlockReason::kWrongScheme:
      DidFinishAsyncSafetyChecks(evaluated_origin, /*may_act=*/false);
  }
}

void ExecutionEngine::DidFinishAsyncSafetyChecks(
    const url::Origin& evaluated_origin,
    bool may_act) {
  TRACE_EVENT0("actor", "ExecutionEngine::DidFinishAsyncSafetyChecks");
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!action_sequence_.empty());

  tabs::TabInterface* tab = GetNextAction().GetTabHandle().Get();
  if (!tab) {
    journal_->Log(GURL::EmptyGURL(), task_->id(), "Act Failed",
                  JournalDetailsBuilder()
                      .AddError("The tab is no longer present")
                      .Build());

    CompleteActions(MakeResult(mojom::ActionResultCode::kTabWentAway,
                               /*requires_page_stabilization=*/false,
                               "The tab is no longer present."),
                    next_action_index_);
    return;
  }

  TaskId task_id = task_->id();
  if (!evaluated_origin.IsSameOriginWith(tab->GetContents()
                                             ->GetPrimaryMainFrame()
                                             ->GetLastCommittedOrigin())) {
    // A cross-origin navigation occurred before we got permission. The result
    // is no longer applicable. For now just fail.
    // TODO(mcnee): Handle this gracefully.
    journal_->Log(GetNextAction().GetURLForJournal(), task_id, "Act Failed",
                  JournalDetailsBuilder()
                      .AddError("Acting after cross-origin navigation occurred")
                      .Build());
    FailedOnTabBeforeToolCreation();
    CompleteActions(MakeResult(mojom::ActionResultCode::kCrossOriginNavigation,
                               /*requires_page_stabilization=*/false,
                               "Acting after cross-origin navigation occurred"),
                    next_action_index_);
    return;
  }

  if (!may_act) {
    journal_->Log(
        GetNextAction().GetURLForJournal(), task_id, "Act Failed",
        JournalDetailsBuilder().AddError("URL blocked for actions").Build());
    FailedOnTabBeforeToolCreation();
    CompleteActions(MakeResult(mojom::ActionResultCode::kUrlBlocked,
                               /*requires_page_stabilization=*/false,
                               "URL blocked for actions"),
                    next_action_index_);
    return;
  }

  ExecuteNextAction();
}

void ExecutionEngine::FailedOnTabBeforeToolCreation() {
  tabs::TabHandle tab = GetNextAction().GetTabHandle();
  journal_->Log(GetNextAction().GetURLForJournal(), task_->id(), "Act Failed",
                JournalDetailsBuilder()
                    .Add("tabId", tab.raw_value())
                    .AddError("Associating tab for failed action")
                    .Build());
  task_->AddTab(tab, base::DoNothing());
}

void ExecutionEngine::ExecuteNextAction() {
  TRACE_EVENT0("actor", "ExecutionEngine::ExecuteNextAction");
  DCHECK_EQ(state_, State::kStartAction);
  CHECK(!action_sequence_.empty());
  CHECK(tool_controller_);

  ++next_action_index_;

  SetState(State::kToolCreateAndVerify);
  tool_controller_->CreateToolAndValidate(
      GetInProgressAction(),
      base::BindOnce(&ExecutionEngine::PostToolCreate, GetWeakPtr()));
}

void ExecutionEngine::PostToolCreate(mojom::ActionResultPtr result) {
  TRACE_EVENT0("actor", "ExecutionEngine::PostToolCreate");
  if (!IsOk(*result)) {
    CompleteActions(std::move(result), InProgressActionIndex());
    return;
  }
  SetState(State::kUiPreInvoke);
  ui_event_dispatcher_->OnPreTool(
      GetInProgressAction(),
      base::BindOnce(&ExecutionEngine::FinishedUiPreInvoke, GetWeakPtr()));
}

void ExecutionEngine::FinishedUiPreInvoke(mojom::ActionResultPtr result) {
  TRACE_EVENT0("actor", "ExecutionEngine::FinishedUiPreInvoke");
  DCHECK_EQ(state_, State::kUiPreInvoke);
  if (!IsOk(*result)) {
    CompleteActions(std::move(result), InProgressActionIndex());
    return;
  }

  SetState(State::kToolInvoke);
  tool_controller_->Invoke(
      base::BindOnce(&ExecutionEngine::FinishedToolInvoke, GetWeakPtr()));
}

void ExecutionEngine::FinishedToolInvoke(mojom::ActionResultPtr result) {
  TRACE_EVENT0("actor", "ExecutionEngine::FinishedToolInvoke");
  DCHECK_EQ(state_, State::kToolInvoke);
  // The current action errored out. Stop the chain.
  std::optional<mojom::ActionResultCode> external_tool_failure_reason;
  std::swap(external_tool_failure_reason, external_tool_failure_reason_);
  if (external_tool_failure_reason) {
    CompleteActions(MakeResult(*external_tool_failure_reason),
                    InProgressActionIndex());
    return;
  }

  if (!IsOk(*result)) {
    CompleteActions(std::move(result), InProgressActionIndex());
    return;
  }

  CHECK(result->execution_end_time);
  base::TimeTicks end_time = base::TimeTicks::Now();
  RecordToolTimings(GetInProgressAction().Name(), end_time - action_start_time_,
                    end_time - *result->execution_end_time);
  action_results_.emplace_back(action_start_time_, end_time, std::move(result));
  SetState(State::kUiPostInvoke);
  ui_event_dispatcher_->OnPostTool(
      GetInProgressAction(),
      base::BindOnce(&ExecutionEngine::FinishedUiPostInvoke, GetWeakPtr()));
}

void ExecutionEngine::FinishedUiPostInvoke(mojom::ActionResultPtr result) {
  TRACE_EVENT0("actor", "ExecutionEngine::FinishedUiPostInvoke");
  DCHECK_EQ(state_, State::kUiPostInvoke);
  CHECK(!action_sequence_.empty());

  if (!IsOk(*result)) {
    CompleteActions(std::move(result), InProgressActionIndex());
    return;
  }

  if (next_action_index_ >= action_sequence_.size()) {
    CompleteActions(MakeOkResult(), std::nullopt);
    return;
  }

  KickOffNextAction();
}

void ExecutionEngine::CompleteActions(mojom::ActionResultPtr result,
                                      std::optional<size_t> action_index) {
  TRACE_EVENT0("actor", "ExecutionEngine::CompleteActions");
  CHECK(!action_sequence_.empty());
  CHECK(act_callback_);

  // If we have not yet appended the action_results for the failed index,
  // append it now.
  if (action_index && action_results_.size() == *action_index) {
    action_results_.emplace_back(action_start_time_, base::TimeTicks::Now(),
                                 result->Clone());
  }

  SetState(State::kComplete);

  if (!IsOk(*result)) {
    GURL url;
    if (action_index) {
      url = action_sequence_[*action_index]->GetURLForJournal();
    }
    journal_->Log(
        url, task_->id(), "Act Failed",
        JournalDetailsBuilder().AddError(ToDebugString(*result)).Build());
  }

  PostTaskForActCallback(std::move(act_callback_), std::move(result),
                         action_index, std::move(action_results_));

  action_sequence_.clear();
  next_action_index_ = 0;
  actions_weak_ptr_factory_.InvalidateWeakPtrs();
}

base::WeakPtr<ExecutionEngine> ExecutionEngine::GetWeakPtr() {
  return actions_weak_ptr_factory_.GetWeakPtr();
}

bool ExecutionEngine::HasActionSequence() const {
  return !action_sequence_.empty();
}

favicon::FaviconService* ExecutionEngine::GetFaviconService() {
  return FaviconServiceFactory::GetForProfile(
      profile_, ServiceAccessType::EXPLICIT_ACCESS);
}

Profile& ExecutionEngine::GetProfile() {
  return *profile_;
}

AggregatedJournal& ExecutionEngine::GetJournal() {
  return *journal_;
}

actor_login::ActorLoginService& ExecutionEngine::GetActorLoginService() {
  return *actor_login_service_;
}

autofill::ActorFormFillingService&
ExecutionEngine::GetActorFormFillingService() {
  return *actor_form_filling_service_;
}

void ExecutionEngine::PromptToSelectCredential(
    const std::vector<actor_login::Credential>& credentials,
    const base::flat_map<std::string, gfx::Image>& icons,
    ToolDelegate::CredentialSelectedCallback callback) {
  TRACE_EVENT0("actor", "ExecutionEngine::PromptToSelectCredential");
  CHECK(!credentials.empty());

  if (!task_->delegate()) {
    // TODO(crbug.com/427817882): Explicit error reason (kNewLonginAttempt).
    std::move(callback).Run(/*selected_credential=*/webui::mojom::
                                SelectCredentialDialogResponse::New());
    return;
  }
  task_->delegate()->RequestToShowCredentialSelectionDialog(
      task_->id(), icons, credentials, std::move(callback));
}

void ExecutionEngine::SetUserSelectedCredential(
    const ToolDelegate::CredentialWithPermission& credential_with_permission) {
  user_selected_credentials_[credential_with_permission.credential
                                 .request_origin] = credential_with_permission;
}

const std::optional<ToolDelegate::CredentialWithPermission>
ExecutionEngine::GetUserSelectedCredential(
    const url::Origin& request_origin) const {
  auto it = user_selected_credentials_.find(request_origin);
  if (it == user_selected_credentials_.end()) {
    return std::nullopt;
  }
  return it->second;
}

void ExecutionEngine::RequestToShowAutofillSuggestions(
    std::vector<autofill::ActorFormFillingRequest> requests,
    ExecutionEngine::AutofillSuggestionSelectedCallback callback) {
  TRACE_EVENT0("actor", "ExecutionEngine::RequestToShowAutofillSuggestions");
  CHECK(!requests.empty());

  if (!task_->delegate()) {
    std::move(callback).Run(
        webui::mojom::SelectAutofillSuggestionsDialogResponse::New(
            task_->id().value(),
            webui::mojom::SelectAutofillSuggestionsDialogResult::NewErrorReason(
                webui::mojom::SelectAutofillSuggestionsDialogErrorReason::
                    kNoActorTaskDelegate)));
    return;
  }
  task_->delegate()->RequestToShowAutofillSuggestionsDialog(
      task_->id(), std::move(requests), std::move(callback));
}

void ExecutionEngine::InterruptFromTool() {
  task_->Interrupt();
}

void ExecutionEngine::UninterruptFromTool() {
  task_->Uninterrupt(ActorTask::State::kActing);
}

void ExecutionEngine::AddWritableMainframeOrigins(
    const ExecutionEngine::AllowedOriginSet& added_writable_mainframe_origins) {
  if (!IsNavigationGatingEnabled()) {
    return;
  }
  for (const auto& origin : added_writable_mainframe_origins) {
    // Intentionally storing a copy of the origin so that ExecutionEngine owns
    // the url::Origin's stored in allowed_navigation_origins_.
    allowed_navigation_origins_.insert(url::Origin(origin));
  }
}

const ToolRequest& ExecutionEngine::GetNextAction() const {
  CHECK_LT(next_action_index_, action_sequence_.size());
  return *action_sequence_.at(next_action_index_).get();
}

size_t ExecutionEngine::InProgressActionIndex() const {
  CHECK(state_ == State::kUiPreInvoke || state_ == State::kToolInvoke ||
        state_ == State::kUiPostInvoke || state_ == State::kToolCreateAndVerify)
      << "Current state is " << StateToString(state_);
  CHECK_GT(next_action_index_, 0ul);
  return next_action_index_ - 1;
}

const ToolRequest& ExecutionEngine::GetInProgressAction() const {
  return *action_sequence_.at(InProgressActionIndex()).get();
}

std::ostream& operator<<(std::ostream& o, const ExecutionEngine::State& s) {
  return o << ExecutionEngine::StateToString(s);
}

}  // namespace actor
