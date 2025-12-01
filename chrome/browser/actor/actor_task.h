// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_ACTOR_TASK_H_
#define CHROME_BROWSER_ACTOR_ACTOR_TASK_H_

#include <iosfwd>
#include <memory>
#include <optional>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/elapsed_timer.h"
#include "base/types/pass_key.h"
#include "build/build_config.h"
#include "chrome/browser/actor/actor_task_delegate.h"
#include "chrome/browser/actor/aggregated_journal.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/common/actor.mojom-forward.h"
#include "chrome/common/actor/task_id.h"
#include "chrome/common/actor_webui.mojom.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/visibility.h"
#include "content/public/common/buildflags.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"

class Profile;
namespace actor {

class ActionTrackerForMetrics;
class ActorKeyedService;
class ExecutionEngine;

namespace ui {
class UiEventDispatcher;
}
struct ActionResultWithLatencyInfo;

// Represents a task that Chrome is executing on behalf of the user.
//
// ActorTask tracks the state of a single interaction session and takes place
// over multiple "turns" (calls to Act()). Browser tabs that are involved in the
// task are added to the set of "controlled" tabs.  ActorTask can be in one of
// three high level states:
//
// * ActorControl: Only the actor is able to interact with controlled tabs
// * UserControl: Only the user is able to interact with controlled tabs
// * Completed: The task is no longer running.
//
// The task is created under actor control. It may be paused or resumed to move
// between actor and user control.
class ActorTask {
 public:
  using ActCallback =
      base::OnceCallback<void(mojom::ActionResultPtr,
                              std::optional<size_t>,
                              std::vector<ActionResultWithLatencyInfo>)>;

  ActorTask() = delete;
  ActorTask(Profile* profile,
            std::unique_ptr<ExecutionEngine> execution_engine,
            std::unique_ptr<ui::UiEventDispatcher> ui_event_dispatcher,
            webui::mojom::TaskOptionsPtr options = nullptr,
            base::WeakPtr<ActorTaskDelegate> delegate = nullptr);
  ActorTask(const ActorTask&) = delete;
  ActorTask& operator=(const ActorTask&) = delete;
  ~ActorTask();

  // Can only be called by ActorKeyedService
  void SetId(base::PassKey<ActorKeyedService>, TaskId id);
  TaskId id() const { return id_; }
  // Can only be called by unit tests.
  void SetIdForTesting(int id);

  const std::string& title() const { return title_; }
  base::WeakPtr<ActorTaskDelegate> delegate() const { return delegate_; }

  // Once `state_` leaves kCreated it should never go back. Once `state_` enters
  // kFinished, kCancelled, or kFailed it should never change. These states are
  // granular, prefer using the Is[Actor|User]Controlled and IsCompleted methods
  // rather than querying `state_` directly.
  //
  // LINT.IfChange(State)
  // These enum values are persisted to logs. Do not renumber or reuse numeric
  // values.
  enum class State {
    kCreated = 0,
    kActing = 1,
    kReflecting = 2,
    kPausedByActor = 3,
    kPausedByUser = 4,
    kCancelled = 5,
    kFinished = 6,
    kWaitingOnUser = 7,
    kFailed = 8,
    kMaxValue = kFailed,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/actor/histograms.xml:ActorTaskState)

  // LINT.IfChange(StoppedReason)
  // The reason a task was stopped.
  enum class StoppedReason {
    kStoppedByUser = 0,
    kTaskComplete = 1,
    kModelError = 2,
    kChromeFailure = 3,
    kTabDetached = 4,
    kShutdown = 5,
    kUserStartedNewChat = 6,
    kUserLoadedPreviousChat = 7,
    kMaxValue = kUserLoadedPreviousChat,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/actor/histograms.xml:StoppedReason,
  // //tools/metrics/histograms/metadata/actor/enums.xml:StoppedReasonEnum)

  State GetState() const;
  // TODO(bokan): This should be private (this class must be in control of its
  // state) but is used by tests. Make the tests friends (or update the tests)
  // and remove it from the public interface.
  void SetState(State new_state);

  base::Time GetEndTime() const;

  void Act(std::vector<std::unique_ptr<ToolRequest>>&& actions,
           ActCallback callback);

  // Sets State to `stop_reason` and cancels any pending actions.
  // TODO(bokan): It's important that Stop only be called from ActorKeyedService
  // since that has to clean up actor tasks. Add a PassKey.
  void Stop(StoppedReason stop_reason);

  // Pause() is called to indicate that either the actor or user is pausing
  // actor actions, determined by the `from_actor` flag. This will cancel any
  // in-progress action.
  void Pause(bool from_actor);

  // Resume() puts the task back into an actor-controlled state. The caller is
  // responsible for updating the actor with the latest state of the browser.
  void Resume();

  // Indicate the task is blocked waiting for user input. The task remains in an
  // actor-controlled state and user interaction is still prevented.
  void Interrupt();

  // Uninterrupt from waiting on user input.
  void Uninterrupt(State resumed_state);

  // Returns true if the task hasn't completed and is under control of the user.
  // That is, the actor cannot send actions and the user is able to interact
  // with the task's tabs. i.e. the task is "paused".
  bool IsUnderUserControl() const;

  // Returns true if the task hasn't completed and is under control of the
  // actor. That is, the user is unable to interact with the task's tabs.
  bool IsUnderActorControl() const;

  // Returns true if the task has completed, either successfully or cancelled.
  bool IsCompleted() const;
  static bool IsCompletedState(State state);

  ExecutionEngine* GetExecutionEngine() const;

  // Add/remove the given TabHandle to the set of tabs this task is operating
  // over and notify the UI if this is a new tab for the task. Added tabs will
  // enter actuation mode and be kept as visible.
  using AddTabCallback = base::OnceCallback<void(mojom::ActionResultPtr)>;
  void AddTab(tabs::TabHandle tab, AddTabCallback callback);
  void RemoveTab(tabs::TabHandle tab);

  // Transient version of the above. The tab will enter the same
  // simulated-visible state but only until the next call to Act. Until then it
  // will always be be included in the LastActedTabs set.
  void ObserveTabOnce(tabs::TabHandle tab_handle);

  // Returns true if the given tab is part of this task's tab set.
  bool HasTab(tabs::TabHandle tab) const;

  // Returns true if the given tab is part of this task's controlled tab set and
  // the task is under actor control.
  bool IsActingOnTab(tabs::TabHandle tab) const;

  using TabHandleSet = absl::flat_hash_set<tabs::TabHandle>;

  // The set of tabs that have been acted on at any point during this task.
  TabHandleSet GetTabs() const;

  // The set of tabs that were acted on by the last call to Act.
  TabHandleSet GetLastActedTabs() const;

  void SetExecutionEngineForTesting(std::unique_ptr<ExecutionEngine> engine);

  base::WeakPtr<ActorTask> GetWeakPtr();

 private:
  class ActorControlledTabState : public content::WebContentsObserver {
   public:
    explicit ActorControlledTabState(ActorTask* task);
    ~ActorControlledTabState() override;

    void SetContents(content::WebContents* web_contents);

    // content::WebContentsObserver overrides
    void PrimaryPageChanged(content::Page& page) override;
    void OnVisibilityChanged(content::Visibility visibility) override;

    // Parent task
    raw_ptr<ActorTask> task;
    // Keeps the tab in "actuation mode". The runner is present when the tab is
    // actively being kept awake and is reset during pause.
    base::ScopedClosureRunner actuation_runner;
    // When a tab is active, external popup menus are disabled. This runner
    // allows external popups to be created again.
#if BUILDFLAG(IS_MAC) && BUILDFLAG(USE_EXTERNAL_POPUP_MENU)
    base::ScopedClosureRunner reenable_external_popups;
#endif  // BUILDFLAG(IS_MAC) && BUILDFLAG(USE_EXTERNAL_POPUP_MENU)
    // Subscription for TabInterface::WillDetach.
    base::CallbackListSubscription will_detach_subscription;
    // Subscription for TabInterface::WillDiscardContents.
    base::CallbackListSubscription content_discarded_subscription;
  };

  // Transitions a tab/contents into a state where only the actor is responsible
  // for interacting with the tab.
  void DidTabEnterActorControl(tabs::TabHandle handle);
  void DidContentsEnterActorControl(ActorControlledTabState* state,
                                    content::WebContents* contents);

  // Transitions the tab from being actor controlled back to user being able to
  // interact with in.
  void DidTabExitActorControl(tabs::TabHandle handle);
  void DidContentsExitActorControl(ActorControlledTabState* state,
                                   content::WebContents* contents);

  // Callback from TabInterface for when the WebContents change.
  void HandleDiscardContents(tabs::TabInterface* tab,
                             content::WebContents* old_contents,
                             content::WebContents* new_contents);

  void OnFinishedAct(mojom::ActionResultPtr result,
                     std::optional<size_t> index_of_failed_action,
                     std::vector<ActionResultWithLatencyInfo> action_results);

  void OnTabWillDetach(tabs::TabInterface* tab,
                       tabs::TabInterface::DetachReason reason);

  void ResetToObserveTabsSet();

  // Recomputes the visible tab. This is necessary to capture the previous
  // visibility state for UpdateVisibilityTimes() when called after
  // ActorControlledTabState::OnVisibilityChanged() is fired.
  void RecomputeHasVisibleTab();
  void UpdateVisibilityTimes();

  State state_ = State::kCreated;
  raw_ptr<Profile> profile_;

  // The time at which the task was created.
  base::TimeTicks create_time_;

  // The time at which the task was completed or cancelled.
  base::Time end_time_;

  std::unique_ptr<ActionTrackerForMetrics> action_tracker_for_metrics_;

  // There are multiple possible execution engines. For now we only support
  // ExecutionEngine.
  std::unique_ptr<ExecutionEngine> execution_engine_;

  std::unique_ptr<ui::UiEventDispatcher> ui_event_dispatcher_;

  TaskId id_;

  base::SafeRef<AggregatedJournal> journal_;

  // The title does not change for the duration of a task.
  const std::string title_;

  // The callback to notify the client of the result of calling Act().
  ActCallback callback_for_act_;

  // A timer for the current state.
  base::ElapsedTimer current_state_timer_;
  // An accumulation of elapsed times for previous "active" states. i.e. the
  // actor is controlling the task and not waiting on a user action.
  base::TimeDelta total_actor_controlled_active_time_;

  // A timer for the current actuation period.
  base::ElapsedTimer visibility_timer_;
  // Whether any of the controlled tabs is visible.
  bool has_visible_tab_ = false;
  // Total time this task has been actuating while a tab was visible.
  base::TimeDelta total_time_visible_;
  // Total time this task has been actuating with no tabs visible.
  base::TimeDelta total_time_not_visible_;

  // A map from a tab's handle to state associated with that tab. The presence
  // of a tab in this map signifies that it is part of this task.
  absl::flat_hash_map<tabs::TabHandle, std::unique_ptr<ActorControlledTabState>>
      controlled_tabs_;

  // An additional set of tabs to capture for observations at the end of an Act
  // turn. Reset at the beginning of each call to Act.
  absl::flat_hash_map<tabs::TabHandle, std::unique_ptr<ActorControlledTabState>>
      to_observe_tabs_;

  // Running number of actions taken in the current state.
  size_t actions_in_current_state_ = 0;
  // Running number of actions this task has taken.
  size_t total_number_of_actions_ = 0;
  // Number of interruptions
  size_t total_number_of_interruptions_ = 0;

  // Once a task is stopped what the reason was.
  std::optional<StoppedReason> stopped_reason_;

  // Delegate for task-related events.
  base::WeakPtr<ActorTaskDelegate> delegate_;

  base::WeakPtrFactory<ui::UiEventDispatcher> ui_weak_ptr_factory_;
  base::WeakPtrFactory<ActorTask> weak_ptr_factory_{this};
};

std::ostream& operator<<(std::ostream& os, const ActorTask::State& state);
std::string ToString(const ActorTask::State& state);

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_ACTOR_TASK_H_
