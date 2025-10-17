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
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_task_delegate.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/common/actor.mojom-forward.h"
#include "chrome/common/actor/task_id.h"
#include "chrome/common/actor_webui.mojom.h"
#include "components/tabs/public/tab_interface.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"

class Profile;
namespace actor {

class ExecutionEngine;
namespace ui {
class UiEventDispatcher;
}
struct ActionResultWithLatencyInfo;

// Represents a task that Chrome is executing on behalf of the user.
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

  // Once state leaves kCreated it should never go back. One state enters
  // kFinished or kCancelled it should never change.

  // LINT.IfChange(State)
  // These enum values are persisted to logs.  Do not renumber or reuse numeric
  // values.
  enum class State {
    kCreated = 0,
    kActing = 1,
    kReflecting = 2,
    kPausedByActor = 3,
    kPausedByUser = 4,
    kCancelled = 5,
    kFinished = 6,
    kMaxValue = kFinished,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/actor/histograms.xml:ActorTaskState)

  State GetState() const;
  void SetState(State new_state);

  base::Time GetEndTime() const;

  void Act(std::vector<std::unique_ptr<ToolRequest>>&& actions,
           ActCallback callback);

  // Sets State to kFinished if `success` is true or to kCancelled if
  // `success` is false and cancels any pending actions.
  void Stop(bool success);

  // Pause() is called to indicate that either the actor or user is pausing
  // server-driven actuation determined by the `from_actor` flag. This will
  // cancel any ongoing actuation.
  void Pause(bool from_actor);

  // Resume() indicates the user wants server-driven actuation to resume. The
  // caller is responsible for sending new state to the server (e.g. APC).
  void Resume();

  bool IsPaused() const;

  bool IsStopped() const;

  bool IsActive() const;

  ExecutionEngine* GetExecutionEngine() const;

  // Add/remove the given TabHandle to the set of tabs this task is operating
  // over and notify the UI if this is a new tab for the task. Added tabs will
  // enter actuation mode and be kept as visible.
  using AddTabCallback = base::OnceCallback<void(mojom::ActionResultPtr)>;
  void AddTab(tabs::TabHandle tab, AddTabCallback callback);
  void RemoveTab(tabs::TabHandle tab);

  // Returns true if the given tab is part of this task's tab set.
  bool HasTab(tabs::TabHandle tab) const;

  // Returns true if the given tab is part of this task's tab set and is in
  // an active (non-paused) state.
  bool IsActingOnTab(tabs::TabHandle tab) const;

  using TabHandleSet = absl::flat_hash_set<tabs::TabHandle>;

  // The set of tabs that have been acted on at any point during this task.
  TabHandleSet GetTabs() const;

  // The set of tabs that were acted on by the last call to Act.
  TabHandleSet GetLastActedTabs() const;

 private:
  class ActingTabState : public content::WebContentsObserver {
   public:
    explicit ActingTabState(ActorTask* task);
    ~ActingTabState() override;

    void SetContents(content::WebContents* web_contents);

    // content::WebContentsObserver overrides
    void PrimaryPageChanged(content::Page& page) override;

    // Parent task
    raw_ptr<ActorTask> task;
    // Keeps the tab in "actuation mode". The runner is present when the tab is
    // actively being kept awake and is reset during pause.
    base::ScopedClosureRunner actuation_runner;
    // Subscription for TabInterface::WillDetach.
    base::CallbackListSubscription will_detach_subscription;
    // Subscription for TabInterface::WillDiscardContents.
    base::CallbackListSubscription content_discarded_subscription;
  };

  // Transitions a tab from an inactive state to an active state.
  void DidTabBecomeActive(tabs::TabHandle handle);

  void DidContentsBecomeActive(ActingTabState* state,
                               content::WebContents* contents);

  // Transitions the tab from an active state to an inactive state.
  void DidTabBecomeInactive(tabs::TabHandle handle);

  void DidContentsBecomeInactive(ActingTabState* state,
                                 content::WebContents* contents);

  // Callback from TabInterface for when the WebContents change.
  void HandleDiscardContents(tabs::TabInterface* tab,
                             content::WebContents* old_contents,
                             content::WebContents* new_contents);

  void OnFinishedAct(ActCallback callback,
                     mojom::ActionResultPtr result,
                     std::optional<size_t> index_of_failed_action,
                     std::vector<ActionResultWithLatencyInfo> action_results);
  void OnTabWillDetach(tabs::TabInterface* tab,
                       tabs::TabInterface::DetachReason reason);

  State state_ = State::kCreated;
  raw_ptr<Profile> profile_;

  // The time at which the task was completed or cancelled.
  base::Time end_time_;

  // There are multiple possible execution engines. For now we only support
  // ExecutionEngine.
  std::unique_ptr<ExecutionEngine> execution_engine_;

  std::unique_ptr<ui::UiEventDispatcher> ui_event_dispatcher_;

  TaskId id_;

  // The title does not change for the duration of a task.
  const std::string title_;

  // A timer for the current state.
  base::ElapsedTimer current_state_timer_;
  // An accumulation of elapsed times for previous "active" states.
  base::TimeDelta total_active_time_;

  // A map from a tab's handle to state associated with that tab. The presence
  // of a tab in this map signifies that it is part of the task.
  absl::flat_hash_map<tabs::TabHandle, std::unique_ptr<ActingTabState>>
      acting_tabs_;

  // Running number of actions taken in the current state.
  size_t actions_in_current_state_ = 0;
  // Running number of actions this task has taken.
  size_t total_number_of_actions_ = 0;

  // Delegate for task-related events.
  base::WeakPtr<ActorTaskDelegate> delegate_;

  base::WeakPtrFactory<ui::UiEventDispatcher> ui_weak_ptr_factory_;
  base::WeakPtrFactory<ActorTask> weak_ptr_factory_{this};
};

std::ostream& operator<<(std::ostream& os, const ActorTask::State& state);
std::string ToString(const ActorTask::State& state);

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_ACTOR_TASK_H_
