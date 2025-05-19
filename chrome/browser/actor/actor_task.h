// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_ACTOR_TASK_H_
#define CHROME_BROWSER_ACTOR_ACTOR_TASK_H_

namespace actor {

// Represents a task that Chrome is executing on behalf of the user.
class ActorTask {
 public:
  ActorTask();
  ActorTask(const ActorTask&) = delete;
  ActorTask& operator=(const ActorTask&) = delete;
  ~ActorTask();

  // Once state leaves kCreated it should never go back. One state enters
  // kFinished it should never change. We may want to add a kCancelled in the
  // future, TBD.
  enum class State { kCreated, kActing, kReflecting, kFinished };

  State GetState() const;

 private:
  State state_ = State::kCreated;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_ACTOR_TASK_H_
