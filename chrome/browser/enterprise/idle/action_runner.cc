// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/idle/action_runner.h"

#include <iterator>

#include "base/functional/bind.h"
#include "base/ranges/algorithm.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

namespace enterprise_idle {

ActionRunner::ActionRunner(Profile* profile, ActionFactory* action_factory)
    : profile_(profile), action_factory_(action_factory) {}

ActionRunner::~ActionRunner() = default;

void ActionRunner::Run() {
  ActionQueue actions = GetActions();
  if (actions.empty())
    return;
  RunNextAction(std::move(actions));
}

ActionRunner::ActionQueue ActionRunner::GetActions() {
  std::vector<ActionType> actions;
  base::ranges::transform(
      profile_->GetPrefs()->GetList(prefs::kIdleTimeoutActions),
      std::back_inserter(actions), [](const base::Value& action) {
        return static_cast<ActionType>(action.GetInt());
      });
  return action_factory_->Build(actions);
}

void ActionRunner::RunNextAction(ActionQueue actions) {
  if (actions.empty())
    return;

  const std::unique_ptr<Action>& action = actions.top();

  action->Run(profile_, base::BindOnce(&ActionRunner::OnActionFinished,
                                       weak_ptr_factory_.GetWeakPtr(),
                                       std::move(actions)));
}

void ActionRunner::OnActionFinished(ActionQueue remaining_actions,
                                    bool succeeded) {
  if (!succeeded)
    return;  // Previous action failed. Abort.

  if (remaining_actions.empty())
    return;  // All done.

  remaining_actions.pop();
  RunNextAction(std::move(remaining_actions));
}

}  // namespace enterprise_idle
