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
#include "base/types/id_type.h"
#include "chrome/browser/actor/actor_features.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_policy_checker.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/browser_action_util.h"
#include "chrome/browser/actor/site_policy.h"
#include "chrome/browser/actor/tools/navigate_tool_request.h"
#include "chrome/browser/actor/tools/tool_controller.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/browser/actor/ui/event_dispatcher.h"
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

void PostTaskForActCallback(
    ActorTask::ActCallback callback,
    mojom::ActionResultPtr result,
    std::optional<size_t> index_of_failed_action,
    std::vector<ActionResultWithLatencyInfo> action_results) {
  UMA_HISTOGRAM_ENUMERATION("Actor.ExecutionEngine.Action.ResultCode",
                            result->code);
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), std::move(result),
                     index_of_failed_action, std::move(action_results)));
}

}  // namespace

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
  UMA_HISTOGRAM_COUNTS_1000("Actor.NavigationGating.AllowListSize",
                            allowed_navigation_origins_.size());
}

void ExecutionEngine::SetOwner(ActorTask* task) {
  task_ = task;
  TRACE_EVENT0("actor", "ExecutionEngine::SetOwner");
  actor_login_service_ = std::make_unique<actor_login::ActorLoginServiceImpl>();
  tool_controller_ = std::make_unique<ToolController>(*task_, *this);
}

void ExecutionEngine::SetState(State state) {
  TRACE_EVENT0("actor", "ExecutionEngine::SetState");
  journal_->Log(GURL(), task_->id(), mojom::JournalTrack::kActor,
                "ExecutionEngine::StateChange",
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
    ExecutionEngine::UserConfirmationDialogCallback callback) {
  if (!base::FeatureList::IsEnabled(kGlicCrossOriginNavigationGating)) {
    return false;
  }
  bool should_apply =
      ShouldGateNavigationInternal(navigation_handle, std::move(callback));
  LogNavigationGating(navigation_handle, should_apply);
  return should_apply;
}

bool ExecutionEngine::ShouldGateNavigationInternal(
    content::NavigationHandle& navigation_handle,
    ExecutionEngine::UserConfirmationDialogCallback callback) {
  base::ScopedUmaHistogramTimer timer(
      "Actor.NavigationGating.TimeElapsedForGating");

  auto navigation_origin = url::Origin::Create(navigation_handle.GetURL());

  // Assumes the initiator origin is safe since it is currently being actuated
  // on.
  const std::optional<url::Origin>& initiator_origin =
      navigation_handle.GetInitiatorOrigin();
  if (initiator_origin &&
      initiator_origin->IsSameOriginWith(navigation_origin)) {
    return false;
  }

  for (const auto& origin : allowed_navigation_origins_) {
    if (origin.IsSameOriginWith(navigation_origin)) {
      return false;
    }
  }

  // Do not prompt user for permission in pre-rendered frames.
  if (navigation_handle.IsInPrerenderedMainFrame()) {
    return true;
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &ExecutionEngine::PromptToConfirmCrossOriginNavigation, GetWeakPtr(),
          navigation_origin,
          base::BindOnce(&ExecutionEngine::OnPromptToConfirmNavigationDecision,
                         GetWeakPtr(), navigation_origin,
                         std::move(callback))));

  return true;
}

void ExecutionEngine::LogNavigationGating(
    content::NavigationHandle& navigation_handle,
    bool applied_gate) {
  UMA_HISTOGRAM_BOOLEAN("Actor.NavigationGating.AppliedGate", applied_gate);

  GURL navigation_url = navigation_handle.GetURL();
  if (const std::optional<url::Origin>& initiator_origin =
          navigation_handle.GetInitiatorOrigin()) {
    UMA_HISTOGRAM_BOOLEAN("Actor.NavigationGating.CrossOrigin",
                          !initiator_origin->IsSameOriginWith(
                              url::Origin::Create(navigation_url)));
    UMA_HISTOGRAM_BOOLEAN("Actor.NavigationGating.CrossSite",
                          !net::SchemefulSite::IsSameSite(
                              initiator_origin->GetURL(), navigation_url));
  }
}

void ExecutionEngine::OnPromptToConfirmNavigationDecision(
    url::Origin navigation_origin,
    ExecutionEngine::UserConfirmationDialogCallback callback,
    webui::mojom::UserConfirmationDialogResponsePtr response) {
  if (response->result->is_permission_granted()) {
    bool permission_granted = response->result->get_permission_granted();
    UMA_HISTOGRAM_BOOLEAN("Actor.NavigationGating.PermissionGranted",
                          permission_granted);
    if (permission_granted) {
      allowed_navigation_origins_.insert(std::move(navigation_origin));
    }
  }
  std::move(callback).Run(std::move(response));
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

  if (!action_sequence_.empty()) {
    journal_->Log(
        actions[0]->GetURLForJournal(), task_->id(),
        mojom::JournalTrack::kActor, "Act Failed",
        JournalDetailsBuilder()
            .AddError(
                "Unable to perform action: task already has action in progress")
            .Build());
    PostTaskForActCallback(std::move(callback),
                           MakeResult(mojom::ActionResultCode::kError,
                                      /*requires_page_stabilization=*/false,
                                      "Task already has action in progress"),
                           std::nullopt, {});
    return;
  }

  act_callback_ = std::move(callback);
  next_action_index_ = 0;

  absl::flat_hash_set<int32_t> acting_tab_handles;

  action_sequence_ = std::move(actions);
  bool origin_gating_enabled =
      base::FeatureList::IsEnabled(kGlicCrossOriginNavigationGating);
  for (const std::unique_ptr<ToolRequest>& action : action_sequence_) {
    CHECK(action);
    if (action->GetTabHandle() != tabs::TabHandle::Null()) {
      acting_tab_handles.insert(action->GetTabHandle().raw_value());
    }
    if (origin_gating_enabled) {
      if (std::optional<url::Origin> maybe_origin =
              action->AssociatedOriginGrant();
          maybe_origin) {
        allowed_navigation_origins_.insert(maybe_origin.value());
      }
    }
  }

  KickOffNextAction(MakeOkResult());
}

void ExecutionEngine::KickOffNextAction(
    mojom::ActionResultPtr init_hooks_result) {
  TRACE_EVENT0("actor", "ExecutionEngine::KickOffNextAction");
  DCHECK(state_ == State::kInit || state_ == State::kUiPostInvoke ||
         state_ == State::kComplete)
      << "Current state is " << StateToString(state_);
  CHECK_LT(next_action_index_, action_sequence_.size());

  // The init hooks errored out.
  if (init_hooks_result && !IsOk(*init_hooks_result)) {
    CompleteActions(std::move(init_hooks_result),
                    /*action_index=*/std::nullopt);
    return;
  }

  SetState(State::kStartAction);

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
    journal_->Log(GURL::EmptyGURL(), task_->id(), mojom::JournalTrack::kActor,
                  "Act Failed",
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
      *tab, *journal_, task_->id(),
      base::BindOnce(
          &ExecutionEngine::DidFinishAsyncSafetyChecks, GetWeakPtr(),
          tab->GetContents()->GetPrimaryMainFrame()->GetLastCommittedOrigin()));
}

void ExecutionEngine::DidFinishAsyncSafetyChecks(
    const url::Origin& evaluated_origin,
    bool may_act) {
  TRACE_EVENT0("actor", "ExecutionEngine::DidFinishAsyncSafetyChecks");
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!action_sequence_.empty());

  tabs::TabInterface* tab = GetNextAction().GetTabHandle().Get();
  if (!tab) {
    journal_->Log(GURL::EmptyGURL(), task_->id(), mojom::JournalTrack::kActor,
                  "Act Failed",
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
    journal_->Log(GetNextAction().GetURLForJournal(), task_id,
                  mojom::JournalTrack::kActor, "Act Failed",
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
        GetNextAction().GetURLForJournal(), task_id,
        mojom::JournalTrack::kActor, "Act Failed",
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
  journal_->Log(GetNextAction().GetURLForJournal(), task_->id(),
                mojom::JournalTrack::kActor, "Act Failed",
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
  action_start_time_ = base::TimeTicks::Now();

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
    action_results_.emplace_back(action_start_time_, base::TimeTicks::Now(),
                                 result->Clone());
    CompleteActions(std::move(result), InProgressActionIndex());
    return;
  }

  action_results_.emplace_back(action_start_time_, base::TimeTicks::Now(),
                               std::move(result));
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

  KickOffNextAction(/*init_hooks_result=*/nullptr);
}

void ExecutionEngine::CompleteActions(mojom::ActionResultPtr result,
                                      std::optional<size_t> action_index) {
  TRACE_EVENT0("actor", "ExecutionEngine::CompleteActions");
  CHECK(!action_sequence_.empty());
  CHECK(act_callback_);

  SetState(State::kComplete);

  if (!IsOk(*result)) {
    GURL url;
    if (action_index) {
      url = action_sequence_[*action_index]->GetURLForJournal();
    }
    journal_->Log(
        url, task_->id(), mojom::JournalTrack::kActor, "Act Failed",
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

void ExecutionEngine::PromptToSelectCredential(
    const std::vector<actor_login::Credential>& credentials,
    const base::flat_map<std::string, gfx::Image>& icons,
    ToolDelegate::CredentialSelectedCallback callback) {
  TRACE_EVENT0("actor", "ExecutionEngine::PromptToSelectCredential");
  CHECK(!credentials.empty());

  // In the same task, another login attempt is made before the previous one
  // responds. Cancel the previous one.
  if (credential_selected_callback_) {
    // TODO(crbug.com/427817882): Explicit error reason (kNewLonginAttempt).
    std::move(credential_selected_callback_)
        .Run(/*selected_credential=*/webui::mojom::
                 SelectCredentialDialogResponse::New());
  }
  credential_selected_callback_ = std::move(callback);

  ActorKeyedService::Get(profile_)
      ->NotifyRequestToShowCredentialSelectionDialog(task_->id(), icons,
                                                     credentials);
}

void ExecutionEngine::SetUserSelectedCredential(
    const actor_login::Credential& credential) {
  user_selected_credentials_[credential.request_origin] = credential;
}

const std::optional<actor_login::Credential>
ExecutionEngine::GetUserSelectedCredential(
    const url::Origin& request_origin) const {
  auto it = user_selected_credentials_.find(request_origin);
  if (it == user_selected_credentials_.end()) {
    return std::nullopt;
  }
  return it->second;
}

void ExecutionEngine::OnCredentialSelected(
    webui::mojom::SelectCredentialDialogResponsePtr response) {
  TRACE_EVENT0("actor", "ExecutionEngine::OnCredentialSelected");
  if (credential_selected_callback_) {
    std::move(credential_selected_callback_).Run(std::move(response));
  }
}

void ExecutionEngine::AddWritableMainframeOrigins(
    const absl::flat_hash_set<url::Origin>& added_writable_mainframe_origins) {
  if (!base::FeatureList::IsEnabled(kGlicCrossOriginNavigationGating)) {
    return;
  }
  for (const auto& origin : added_writable_mainframe_origins) {
    // Intentionally storing a copy of the origin so that ExecutionEngine owns
    // the url::Origin's stored in allowed_navigation_origins_.
    allowed_navigation_origins_.insert(url::Origin(origin));
  }
}

void ExecutionEngine::PromptToConfirmCrossOriginNavigation(
    const url::Origin& navigation_origin,
    ExecutionEngine::UserConfirmationDialogCallback callback) {
  PromptUserForConfirmationInternal(
      navigation_origin, /*download_url=*/std::nullopt, std::move(callback));
}

void ExecutionEngine::PromptToConfirmDownload(
    int32_t download_id,
    ExecutionEngine::UserConfirmationDialogCallback callback) {
  PromptUserForConfirmationInternal(/*navigation_origin=*/std::nullopt,
                                    download_id, std::move(callback));
}

void ExecutionEngine::PromptUserForConfirmationInternal(
    const std::optional<url::Origin>& navigation_origin,
    const std::optional<int32_t> download_id,
    ExecutionEngine::UserConfirmationDialogCallback callback) {
  if (user_confirmation_callback_) {
    std::move(user_confirmation_callback_)
        .Run(webui::mojom::UserConfirmationDialogResponse::New(
            webui::mojom::UserConfirmationDialogResult::NewErrorReason(
                webui::mojom::UserConfirmationDialogErrorReason::
                    kPreemptedByNewRequest)));
  }
  user_confirmation_callback_ = std::move(callback);
  ActorKeyedService::Get(profile_)->NotifyRequestToShowUserConfirmationDialog(
      task_->id(), navigation_origin, download_id);
}

void ExecutionEngine::OnUserConfirmation(
    webui::mojom::UserConfirmationDialogResponsePtr response) {
  CHECK(user_confirmation_callback_);
  std::move(user_confirmation_callback_).Run(std::move(response));
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
