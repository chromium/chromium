// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_EXECUTION_ENGINE_H_
#define CHROME_BROWSER_ACTOR_EXECUTION_ENGINE_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/safe_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/types/id_type.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/aggregated_journal.h"
#include "chrome/browser/actor/site_policy.h"
#include "chrome/browser/actor/tools/tool_controller.h"
#include "chrome/browser/actor/tools/tool_delegate.h"
#include "chrome/browser/password_manager/actor_login/actor_login_service.h"
#include "chrome/common/actor.mojom-forward.h"
#include "chrome/common/actor/task_id.h"
#include "components/autofill/core/browser/integrators/glic/actor_form_filling_types.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"

class Profile;

namespace affiliations {
struct Facet;
}  // namespace affiliations

namespace content {
class NavigationHandle;
}

namespace url {
class Origin;
}  // namespace url

namespace actor {

class ActorTask;
class ToolRequest;
namespace ui {
class UiEventDispatcher;
}

// Coordinates the execution of a multi-step task.
class ExecutionEngine : public ToolDelegate {
 public:
  // State machine (success case)
  //
  //    Init
  //     |
  //     v
  // StartAction -> ToolCreateAndVerify ->
  //     ^          UiPreInvoke -> ToolInvoke -> UiPostInvoke -> Complete
  //     |                                           |              |
  //     |___________________________________________|______________|
  //
  // Complete may also be reached directly from other states in case of error.
  enum class State {
    kInit = 0,
    kStartAction,
    kToolCreateAndVerify,
    kUiPreInvoke,
    kToolInvoke,
    kUiPostInvoke,
    kComplete,
  };

  // This enum represents the possible outcomes of the synchronous part of the
  // navigation gating logic.
  // LINT.IfChange(GatingDecision)
  // These enum values are persisted to logs.  Do not renumber or reuse numeric
  // values.
  enum class GatingDecision {
    // The source origin and navigation origin are the same and should not be
    // gated.
    kAllowSameOrigin = 0,
    // The navigation is allowed by the static allow-list.
    kAllowByStaticList = 1,
    // The navigation is blocked by the static block-list. The user will not be
    // prompted for confirmation.
    kBlockByStaticList = 2,
    // The navigation is not on any allowlist or blocklist and requires an
    // asynchronous check to determine the final outcome.
    kNeedsAsyncCheck = 3,
    kMaxValue = kNeedsAsyncCheck,
  };

  // LINT.ThenChange(//tools/metrics/histograms/metadata/actor/enums.xml:GatingDecision)

  class StateObserver : public base::CheckedObserver {
   public:
    ~StateObserver() override = default;
    virtual void OnStateChanged(State old_state, State new_state) = 0;
  };

  explicit ExecutionEngine(Profile* profile);
  ExecutionEngine(const ExecutionEngine&) = delete;
  ExecutionEngine& operator=(const ExecutionEngine&) = delete;
  ~ExecutionEngine() override;

  static std::unique_ptr<ExecutionEngine> CreateForTesting(
      Profile* profile,
      std::unique_ptr<ui::UiEventDispatcher> ui_event_dispatcher);

  // This cannot be in the constructor as we first construct the
  // ExecutionEngine, then the ActorTask.
  void SetOwner(ActorTask* task);

  // Cancels any in-progress actions with the reason: "kTaskPaused".
  void CancelOngoingActions(mojom::ActionResultCode reason);

  // If there is an ongoing tool request, treat it as having failed with the
  // given reason.
  void FailCurrentTool(mojom::ActionResultCode reason);

  // Performs the given tool actions and invokes the callback when completed.
  void Act(std::vector<std::unique_ptr<ToolRequest>>&& actions,
           ActorTask::ActCallback callback);

  // Invalidated anytime `action_sequence_` is reset.
  base::WeakPtr<ExecutionEngine> GetWeakPtr();

  bool HasActionSequence() const;

  // ToolDelegate:
  Profile& GetProfile() override;
  AggregatedJournal& GetJournal() override;
  favicon::FaviconService* GetFaviconService() override;
  void IsAcceptableNavigationDestination(
      const GURL& url,
      DecisionCallbackWithReason callback) override;
  actor_login::ActorLoginService& GetActorLoginService() override;
  autofill::ActorFormFillingService& GetActorFormFillingService() override;
  void PromptToSelectCredential(
      const std::vector<actor_login::Credential>& credentials,
      const base::flat_map<std::string, gfx::Image>& icons,
      ToolDelegate::CredentialSelectedCallback callback) override;
  void SetUserSelectedCredential(
      const CredentialWithPermission& credential,
      base::OnceClosure affiliations_fetched) override;
  const std::optional<CredentialWithPermission> GetUserSelectedCredential(
      const url::Origin& request_origin) const override;
  void RequestToShowAutofillSuggestions(
      std::vector<autofill::ActorFormFillingRequest> requests,
      AutofillSuggestionSelectedCallback callback) override;
  void InterruptFromTool() override;
  void UninterruptFromTool() override;

  using AllowedOriginSet = absl::flat_hash_set<url::Origin>;
  void AddWritableMainframeOrigins(
      const AllowedOriginSet& added_writable_mainframe_origins);

  // Callback invoked when ConfirmCrossOriginNavigation, which spawns an IPC to
  // the web client, receives its response. This callback gets a boolean
  // indicating if navigation should continue.
  using NavigationDecisionCallback =
      base::OnceCallback<void(bool may_continue)>;

  // Returns a boolean indicating if ActorNavigationThrottle should defer a
  // navigation until the decision callback is invoked. This method can only
  // be called on the primary main frame or a prerendered main frame.
  bool ShouldGateNavigation(content::NavigationHandle& navigation_handle,
                            NavigationDecisionCallback callback);

  static std::string StateToString(State state);

  void OnMayActOnTabDecision(const url::Origin& evaluated_origin,
                             MayActOnUrlBlockReason block_reason);

  void UserTakeover(mojom::ActionResultCode takeover_response_code,
                    base::OnceCallback<void(bool)> callback);

  void RunUserTakeoverCallbackIfExists(bool should_cancel);

  void set_user_take_over_result(
      std::optional<mojom::ActionResultCode> user_takeover_result) {
    user_takeover_result_ = user_takeover_result;
  }

  std::optional<mojom::ActionResultCode> user_take_over_result() const {
    return user_takeover_result_;
  }

  const base::flat_map<url::Origin, url::Origin>&
  GetAffiliatedOriginMapForTesting() const {
    return affiliated_origin_map_;
  }

  void AddObserver(StateObserver* observer);

  void RemoveObserver(StateObserver* observer);

 private:
  class NewTabWebContentsObserver;
  // Used by tests only.
  ExecutionEngine(Profile* profile,
                  std::unique_ptr<ui::UiEventDispatcher> ui_event_dispatcher);

  void SetState(State state);

  // Starts the next action by calling SafetyChecksForNextAction(). Must only be
  // called if there is a next action.
  void KickOffNextAction();

  // Performs safety checks for next action. This is asynchronous.
  void SafetyChecksForNextAction();

  // Performs synchronous safety checks for the next action. If everything
  // passes calls tool_controller_.Invoke().
  void DidFinishAsyncSafetyChecks(const url::Origin& evaluated_origin,
                                  bool may_act);

  // If a failure occurs before the next action starts, we associate the tab
  // that the action would have acted on with the task, so that we can provide
  // tab observations back to the client.
  void FailedOnTabBeforeToolCreation();

  // Synchronously executes the next action. There are several types of actions,
  // including renderer-scoped actions, tab-scoped actions, and global actions.
  void ExecuteNextAction();

  // Called each time an action finishes.
  void PostToolCreate(mojom::ActionResultPtr result);
  void FinishedUiPreInvoke(mojom::ActionResultPtr result);
  void FinishedToolInvoke(mojom::ActionResultPtr result);
  void FinishedUiPostInvoke(mojom::ActionResultPtr result);

  void CompleteActions(mojom::ActionResultPtr result,
                       std::optional<size_t> action_index);

  // Returns the next action that will be started when ExecuteNextAction is
  // reached.
  const ToolRequest& GetNextAction() const;

  // Processes the affiliation service results for the given `source_origin`.
  // and saves it into `affiliated_origin_map_`.
  void OnAffiliationsReceived(const url::Origin& source_origin,
                              base::OnceClosure affiliations_fetched,
                              const std::vector<affiliations::Facet>& results,
                              bool success);

  // Returns the index / action that was last executed and is still in progress.
  // It is an error to call this when an action is not in progress.
  size_t InProgressActionIndex() const;
  const ToolRequest& GetInProgressAction() const;

  // `std::nullopt` is returned when the decision to gate the navigation is done
  // async.
  GatingDecision ShouldGateNavigationInternal(
      content::NavigationHandle& navigation_handle,
      NavigationDecisionCallback callback);
  void LogNavigationGating(const std::optional<url::Origin>& initiator_origin,
                           const GURL& navigation_url,
                           bool applied_gate);

  // Returns the highest-priority navigation gating decision. Prioritizes
  // blocking navigations over allowing (except on same origin navigations).
  GatingDecision DetermineGatingDecision(const GURL& source_url,
                                         const GURL& destination_url) const;

  void CheckNavigationBlocklist(
      const std::optional<url::Origin>& initiator_origin,
      const GURL& navigation_url,
      bool skip_prompt,
      NavigationDecisionCallback callback);
  void OnNavigationBlocklistDecision(
      const std::optional<url::Origin> initiator_origin,
      const GURL navigation_url,
      bool skip_prompt,
      NavigationDecisionCallback callback,
      bool not_on_blocklist);

  // Called when the browser detects the actor needs to confirm a
  // client-side-initiated navigation to a novel origin.
  void HandleNavigationToNewOrigin(
      const url::Origin& navigation_origin,
      ExecutionEngine::NavigationDecisionCallback callback);

  void SendNavigationConfirmationRequest(const url::Origin& navigation_origin,
                                         NavigationDecisionCallback callback);
  void OnNavigationConfirmationDecision(
      url::Origin navigation_origin,
      NavigationDecisionCallback callback,
      webui::mojom::NavigationConfirmationResponsePtr response);

  // Called when the browser detects the actor navigating to an origin in the
  // blocklist. The web client should confirm with the user that the actor is
  // allowed to navigate to this origin.
  // This may also be called when the browser detects the actor navigating to
  // a novel origin when `kGlicPromptUserForNavigationToNewOrigins` is enabled.
  void SendUserConfirmationDialogRequest(const url::Origin& navigation_origin,
                                         bool for_blocklisted_origin,
                                         NavigationDecisionCallback callback);
  void OnPromptUserToConfirmNavigationDecision(
      url::Origin navigation_origin,
      bool for_blocklisted_origin,
      NavigationDecisionCallback callback,
      webui::mojom::UserConfirmationDialogResponsePtr response);

  State state_ = State::kInit;

  static std::optional<base::TimeDelta> action_observation_delay_for_testing_;

  raw_ptr<Profile> profile_;
  base::SafeRef<AggregatedJournal> journal_;

  // Owns `this`.
  raw_ptr<ActorTask> task_;

  // Created when task_ is set. Handles execution details for an individual tool
  // request.
  std::unique_ptr<ToolController> tool_controller_;
  std::unique_ptr<actor_login::ActorLoginService> actor_login_service_;
  std::unique_ptr<autofill::ActorFormFillingService>
      actor_form_filling_service_;
  std::unique_ptr<ui::UiEventDispatcher> ui_event_dispatcher_;

  base::flat_map<url::Origin, url::Origin> affiliated_origin_map_;

  std::vector<std::unique_ptr<ToolRequest>> action_sequence_;
  ActorTask::ActCallback act_callback_;

  // The index of the next action that will be started when ExecuteNextAction is
  // reached.
  size_t next_action_index_ = 0;
  base::TimeTicks action_start_time_;

  // If set, the currently executing tool should be considered failed once it
  // completes.
  std::optional<mojom::ActionResultCode> external_tool_failure_reason_;

  // The results for actions so far.
  std::vector<ActionResultWithLatencyInfo> action_results_;

  // Origins which the browser is allowed to navigate to under actor control
  // without needing to confirm the navigation with the web client. This set can
  // have origins added to it by the server actions or by confirming the new
  // origin with the model or user. Sensitive origins that are on the
  // optimization guide blocklist are not exempt by this list.
  AllowedOriginSet allowed_navigation_origins_;
  // Separate allowlist for sensitive origins on the optimization guide
  // blocklist. We cache these origins separately to not double prompt the user
  // when they already confirmed the actor can interact with the origin.
  AllowedOriginSet user_confirmed_blocklisted_origins_;

  // For multi-step login, this is the credential that the user has chosen to
  // allow the actor to use. The key is the
  // `Credential::request_origin`.
  base::flat_map<url::Origin, CredentialWithPermission>
      user_selected_credentials_;

  base::OnceCallback<void(bool /*should_cancel*/)> user_takeover_callback_;
  std::optional<mojom::ActionResultCode> user_takeover_result_;

  base::ObserverList<StateObserver> observers_;

  SEQUENCE_CHECKER(sequence_checker_);

  // Normally, a WeakPtrFactory only invalidates its WeakPtrs when the object is
  // destroyed. However, this class invalidates WeakPtrs anytime a new set of
  // actions is passed in. This effectively cancels any ongoing async actions.
  base::WeakPtrFactory<ExecutionEngine> actions_weak_ptr_factory_{this};
};

std::ostream& operator<<(std::ostream& o, const ExecutionEngine::State& s);

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_EXECUTION_ENGINE_H_
