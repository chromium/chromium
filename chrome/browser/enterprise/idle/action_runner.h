// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_IDLE_ACTION_RUNNER_H_
#define CHROME_BROWSER_ENTERPRISE_IDLE_ACTION_RUNNER_H_

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/enterprise/idle/action.h"

class Profile;

namespace enterprise_idle {

// Runs actions specified by the IdleTimeoutActions policy. Wrapper around
// Action that handles asynchronicity, and runs them in order of priority.
//
// One per profile. Owned by `IdleService`.
class ActionRunner {
 public:
  using ActionQueue = ActionFactory::ActionQueue;

  ActionRunner(Profile* profile, ActionFactory* action_factory);
  ~ActionRunner();

  ActionRunner(const ActionRunner&) = delete;
  ActionRunner& operator=(const ActionRunner&) = delete;
  ActionRunner(ActionRunner&&) = delete;
  ActionRunner& operator=(ActionRunner&&) = delete;

  // Runs all the actions, in order of priority. Actions are run sequentially,
  // not in parallel. If an action fails for whatever reason, skips the
  // remaining actions.
  void Run();

 private:
  // Defines the set of actions to be run when Run() is called. `actions` is
  // typically the value of a *Actions policy, e.g. IdleTimeoutActions.
  ActionQueue GetActions();

  // Helper function for Run() and OnActionFinished(). Run the first action in
  // the queue, and schedules the rest of the actions to run when the first
  // action is done.
  void RunNextAction(ActionQueue actions);

  // Callback used by RunSingleAction. Runs when an action finishes, and kicks
  // off the next action (if there's one).
  void OnActionFinished(ActionQueue remaining_actions, bool succeeded);

  base::TimeTicks actions_start_time_;
  raw_ptr<Profile> profile_;
  raw_ptr<ActionFactory> action_factory_;
  base::WeakPtrFactory<ActionRunner> weak_ptr_factory_{this};
};

}  // namespace enterprise_idle

#endif  // CHROME_BROWSER_ENTERPRISE_IDLE_ACTION_RUNNER_H_
