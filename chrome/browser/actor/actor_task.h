// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_ACTOR_TASK_H_
#define CHROME_BROWSER_ACTOR_ACTOR_TASK_H_

#include <iosfwd>
#include <memory>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/types/pass_key.h"
#include "chrome/browser/actor/task_id.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/common/actor.mojom-forward.h"
#include "components/tabs/public/tab_interface.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"

class Profile;
namespace actor {

class ActorKeyedService;
class ExecutionEngine;
namespace ui {
class UiEventDispatcher;
}

// Represents a task that Chrome is executing on behalf of the user.
class ActorTask {
 public:
  using ActCallback =
      base::OnceCallback<void(mojom::ActionResultPtr, std::optional<size_t>)>;

  ActorTask() = delete;
  ActorTask(Profile* profile,
            std::unique_ptr<ExecutionEngine> execution_engine,
            std::unique_ptr<ui::UiEventDispatcher> ui_event_dispatcher);
  ActorTask(const ActorTask&) = delete;
  ActorTask& operator=(const ActorTask&) = delete;
  ~ActorTask();

  // Can only be called by ActorKeyedService
  void SetId(base::PassKey<ActorKeyedService>, TaskId id);
  TaskId id() const { return id_; }
  // Can only be called by unit tests.
  void SetIdForTesting(int id);

  // Once state leaves kCreated it should never go back. One state enters
  // kFinished it should never change. We may want to add a kCancelled in the
  // future, TBD.
  enum class State {
    kCreated,
    kActing,
    kReflecting,
    kPausedByClient,
    kFinished
  };

  State GetState() const;
  void SetState(State state);

  base::Time GetEndTime() const;

  void Act(std::vector<std::unique_ptr<ToolRequest>>&& actions,
           ActCallback callback);

  // Sets State to kFinished and cancels any pending actions.
  void Stop();

  // Pause() is called to indicate that the user is pausing server-driven
  // actuation. This will cancel any ongoing actuation.
  void Pause();

  // Resume() indicates the user wants server-driven actuation to resume. The
  // caller is responsible for sending new state to the server (e.g. APC).
  void Resume();

  bool IsPaused() const;

  ExecutionEngine* GetExecutionEngine() const;

  // Add/remove the given TabHandle to the set of tabs this task is operating
  // over and notify the UI if this is a new tab for the task. Added tabs will
  // enter actuation mode and be kept as visible.
  using AddTabCallback = base::OnceCallback<void(mojom::ActionResultPtr)>;
  void AddTab(tabs::TabHandle tab, AddTabCallback callback);
  void RemoveTab(tabs::TabHandle tab);

  // Returns true if the given tab is part of this task's acting set.
  bool IsActingOnTab(tabs::TabHandle tab) const;

  // Returns the tab to use to capture new context observations after an
  // execution turn. In the future this will be extended to multiple tabs and
  // windows. Currently this returns the first live tab in the set, since the
  // actor framework doesn't yet support multi-tab.
  // TODO(crbug.com/411462297): This will be replaced by GetTabs soon.
  tabs::TabInterface* GetTabForObservation() const;

  // The set of tabs that have been acted on at any point during this task.
  const absl::flat_hash_set<tabs::TabHandle>& GetTabs() const {
    return tab_handles_;
  }

  // The set of tabs that were acted on by the last call to Act.
  const absl::flat_hash_set<tabs::TabHandle>& GetLastActedTabs() const {
    // TODO(bokan): Currently the client only acts on a single tab but this
    // should track which tabs were acted on in the last call to Act.
    return tab_handles_;
  }

 private:
  void OnFinishedAct(ActCallback callback,
                     mojom::ActionResultPtr result,
                     std::optional<size_t> index_of_failed_action);

  State state_ = State::kCreated;
  raw_ptr<Profile> profile_;

  // The time at which the task was completed or cancelled.
  base::Time end_time_;

  // There are multiple possible execution engines. For now we only support
  // ExecutionEngine.
  std::unique_ptr<ExecutionEngine> execution_engine_;

  std::unique_ptr<ui::UiEventDispatcher> ui_event_dispatcher_;

  TaskId id_;

  // The set of all tabs this task has acted upon.
  absl::flat_hash_set<tabs::TabHandle> tab_handles_;

  // A map from a tab's handle to a ScopedClosureRunner that keeps the tab
  // in "actuation mode". This is released when the tab is removed from the
  // task.
  absl::flat_hash_map<tabs::TabHandle, base::ScopedClosureRunner>
      actuation_mode_runners_;

  base::WeakPtrFactory<ui::UiEventDispatcher> ui_weak_ptr_factory_;
  base::WeakPtrFactory<ActorTask> weak_ptr_factory_{this};
};

std::ostream& operator<<(std::ostream& os, const ActorTask::State& state);
std::string ToString(const ActorTask::State& state);

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_ACTOR_TASK_H_
