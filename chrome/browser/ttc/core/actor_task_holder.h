// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_CORE_ACTOR_TASK_HOLDER_H_
#define CHROME_BROWSER_TTC_CORE_ACTOR_TASK_HOLDER_H_

#include "base/memory/raw_ref.h"
#include "components/actor/core/task_id.h"

class Profile;

namespace ttc {

class SessionControllerImpl;

// Creates and owns the actor task for a TTC session. The task is started on
// construction and stopped on destruction.
class ActorTaskHolder {
 public:
  explicit ActorTaskHolder(SessionControllerImpl& session_controller);
  ActorTaskHolder(const ActorTaskHolder&) = delete;
  ActorTaskHolder& operator=(const ActorTaskHolder&) = delete;
  ~ActorTaskHolder();

  // Creates the actor task used to invoke tools, if one isn't already active.
  actor::TaskId EnsureTaskCreated();

  actor::TaskId task_id() const { return task_id_; }

 private:
  Profile* GetProfile();

  // Owns this object.
  const raw_ref<SessionControllerImpl> session_controller_;

  // The task can be stopped outside of this class, e.g. when the user closes a
  // tab it acted on (see EnsureTaskCreated()), in which case it's replaced by a
  // new one.
  actor::TaskId task_id_;
};

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_CORE_ACTOR_TASK_HOLDER_H_
