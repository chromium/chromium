// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_ACTOR_TASK_H_
#define CHROME_BROWSER_ACTOR_ACTOR_TASK_H_

#include <iosfwd>
#include <memory>

#include "base/types/pass_key.h"
#include "chrome/browser/actor/task_id.h"

namespace actor {

class ActorKeyedService;
class ExecutionEngine;

// Represents a task that Chrome is executing on behalf of the user.
class ActorTask {
 public:
  ActorTask();
  explicit ActorTask(std::unique_ptr<ExecutionEngine> execution_engine);
  ActorTask(const ActorTask&) = delete;
  ActorTask& operator=(const ActorTask&) = delete;
  ~ActorTask();

  // Can only be called by ActorKeyedService
  void SetId(base::PassKey<ActorKeyedService>, TaskId id);
  TaskId id() { return id_; }

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

 private:
  State state_ = State::kCreated;

  // There are multiple possible execution engines. For now we only support
  // ExecutionEngine.
  std::unique_ptr<ExecutionEngine> execution_engine_;

  TaskId id_;
};

std::ostream& operator<<(std::ostream& os, const ActorTask::State& state);

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_ACTOR_TASK_H_
