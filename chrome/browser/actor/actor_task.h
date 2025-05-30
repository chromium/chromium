// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_ACTOR_TASK_H_
#define CHROME_BROWSER_ACTOR_ACTOR_TASK_H_

#include <memory>

namespace actor {

class ActorCoordinator;

// Represents a task that Chrome is executing on behalf of the user.
class ActorTask {
 public:
  ActorTask();
  explicit ActorTask(std::unique_ptr<ActorCoordinator> actor_coordinator);
  ActorTask(const ActorTask&) = delete;
  ActorTask& operator=(const ActorTask&) = delete;
  ~ActorTask();

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

  bool IsPaused() const { return GetState() == State::kPausedByClient; }

  ActorCoordinator* GetActorCoordinator() const;

 private:
  State state_ = State::kCreated;

  // There are multiple possible execution engines. For now we only support
  // ActorCoordinator.
  std::unique_ptr<ActorCoordinator> actor_coordinator_;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_ACTOR_TASK_H_
