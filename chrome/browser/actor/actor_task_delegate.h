// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_ACTOR_TASK_DELEGATE_H_
#define CHROME_BROWSER_ACTOR_ACTOR_TASK_DELEGATE_H_

#include "chrome/common/actor/task_id.h"
#include "components/tabs/public/tab_interface.h"

namespace actor {

class ActorTask;

// Delegate interface for ActorTask to communicate with other classes.
class ActorTaskDelegate {
 public:
  virtual ~ActorTaskDelegate() = default;

  // Called asynchronously when a tab is added to a task.
  virtual void OnTabAddedToTask(
      TaskId task_id,
      const tabs::TabInterface::Handle& tab_handle) = 0;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_ACTOR_TASK_DELEGATE_H_
