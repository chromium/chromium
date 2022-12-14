// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/idle/action.h"

#include <cstring>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_list.h"
#include "base/containers/flat_map.h"
#include "base/ranges/algorithm.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/enterprise/idle/action_runner.h"
#include "chrome/browser/enterprise/idle/browser_closer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/profile_picker.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

namespace enterprise_idle {

namespace {

// Wrapper Action for BrowserCloser.
class CloseBrowsersAction : public Action {
 public:
  CloseBrowsersAction() : Action(ActionType::kCloseBrowsers) {}

  void Run(Profile* profile, Continuation continuation) override {
    base::TimeDelta timeout =
        profile->GetPrefs()->GetTimeDelta(prefs::kIdleTimeout);
    continuation_ = std::move(continuation);
    subscription_ = BrowserCloser::GetInstance()->ShowDialogAndCloseBrowsers(
        profile, timeout,
        base::BindOnce(&CloseBrowsersAction::OnCloseFinished,
                       base::Unretained(this)));
  }

 private:
  void OnCloseFinished(BrowserCloser::CloseResult result) {
    std::move(continuation_)
        .Run(result == BrowserCloser::CloseResult::kSuccess);
  }

  Action::Continuation continuation_;
  base::CallbackListSubscription subscription_;
};

// Action that shows the Profile Picker.
class ShowProfilePickerAction : public Action {
 public:
  ShowProfilePickerAction() : Action(ActionType::kShowProfilePicker) {}

  void Run(Profile* profile, Continuation continuation) override {
    ProfilePicker::Show(ProfilePicker::Params::FromEntryPoint(
        ProfilePicker::EntryPoint::kProfileIdle));
    std::move(continuation).Run(true);
  }
};

}  // namespace

const ActionTypeMapEntry kActionTypeMap[] = {
    {"close_browsers", ActionType::kCloseBrowsers},
    {"show_profile_picker", ActionType::kShowProfilePicker},
};
const size_t kActionTypeMapSize = std::size(kActionTypeMap);

Action::Action(ActionType action_type) : action_type_(action_type) {}

Action::~Action() = default;

bool ActionFactory::CompareActionsByPriority::operator()(
    const std::unique_ptr<Action>& a,
    const std::unique_ptr<Action>& b) const {
  return a->priority() > b->priority();
}

// static
ActionFactory* ActionFactory::GetInstance() {
  static ActionFactory instance;
  return &instance;
}

ActionFactory::ActionQueue ActionFactory::Build(
    const std::vector<ActionType>& action_types) {
  ActionQueue actions;

  for (auto action_type : action_types) {
    switch (action_type) {
      case ActionType::kCloseBrowsers:
        actions.push(std::make_unique<CloseBrowsersAction>());
        break;
      case ActionType::kShowProfilePicker:
        actions.push(std::make_unique<ShowProfilePickerAction>());
        break;
      default:
        // TODO(crbug.com/1316551): Perform validation in the `PolicyHandler`.
        NOTREACHED();
    }
  }

  return actions;
}

ActionFactory::ActionFactory() = default;

}  // namespace enterprise_idle
