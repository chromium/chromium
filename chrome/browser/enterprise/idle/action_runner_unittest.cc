// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/idle/action_runner.h"

#include "base/memory/raw_ptr.h"
#include "base/test/gmock_callback_support.h"
#include "chrome/browser/enterprise/idle/action.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_idle {

namespace {

struct RunEntry {
  raw_ptr<Profile> profile;
  base::flat_set<std::string> action_names;
};

class FakeActionFactory : public ActionFactory {
 public:
  FakeActionFactory() = default;

  ActionQueue Build(const std::vector<ActionType>& action_types) override {
    ActionQueue actions;
    for (ActionType action_type : action_types) {
      auto it = associations_.find(action_type);
      if (it != associations_.end()) {
        actions.push(std::move(it->second));
        associations_.erase(it);
      }
    }
    return actions;
  }

  void Associate(ActionType action_type, std::unique_ptr<Action> action) {
    associations_[action_type] = std::move(action);
  }

 private:
  std::map<ActionType, std::unique_ptr<Action>> associations_;
};

class MockAction : public Action {
 public:
  explicit MockAction(ActionType action_type) : Action(action_type) {}

  MOCK_METHOD2(Run, void(Profile*, Continuation));
};

// testing::InvokeArgument<N> does not work with base::OnceCallback, so we
// define our own gMock action to run the 2nd argument.
ACTION_P(RunContinuation, success) {
  std::move(const_cast<Action::Continuation&>(arg1)).Run(success);
}

}  // namespace

using testing::_;

// Tests that actions are run in sequence, in order of priority.
TEST(IdleActionRunnerTest, RunsActionsInSequence) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  FakeActionFactory action_factory;
  ActionRunner runner(&profile, &action_factory);

  base::Value::List actions;
  actions.Append(static_cast<int>(ActionType::kCloseBrowsers));
  actions.Append(static_cast<int>(ActionType::kShowProfilePicker));
  profile.GetPrefs()->SetList(prefs::kIdleTimeoutActions, std::move(actions));

  auto close_browsers =
      std::make_unique<MockAction>(ActionType::kCloseBrowsers);
  auto show_profile_picker =
      std::make_unique<MockAction>(ActionType::kShowProfilePicker);
  testing::InSequence in_sequence;
  EXPECT_CALL(*close_browsers, Run(&profile, _))
      .WillOnce(RunContinuation(true));
  EXPECT_CALL(*show_profile_picker, Run(&profile, _))
      .WillOnce(RunContinuation(true));

  action_factory.Associate(ActionType::kCloseBrowsers,
                           std::move(close_browsers));
  action_factory.Associate(ActionType::kShowProfilePicker,
                           std::move(show_profile_picker));
  runner.Run();
}

// Tests that the order of actions in the pref doesn't matter. They still run
// by order of priority.
TEST(IdleActionRunnerTest, PrefOrderDoesNotMatter) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  FakeActionFactory action_factory;
  ActionRunner runner(&profile, &action_factory);

  base::Value::List actions;
  actions.Append(static_cast<int>(ActionType::kCloseBrowsers));
  actions.Append(static_cast<int>(ActionType::kShowProfilePicker));
  profile.GetPrefs()->SetList(prefs::kIdleTimeoutActions, std::move(actions));

  auto close_browsers =
      std::make_unique<MockAction>(ActionType::kCloseBrowsers);
  auto show_profile_picker =
      std::make_unique<MockAction>(ActionType::kShowProfilePicker);
  testing::InSequence in_sequence;
  EXPECT_CALL(*close_browsers, Run(&profile, _))
      .WillOnce(RunContinuation(true));
  EXPECT_CALL(*show_profile_picker, Run(&profile, _))
      .WillOnce(RunContinuation(true));

  action_factory.Associate(ActionType::kCloseBrowsers,
                           std::move(close_browsers));
  action_factory.Associate(ActionType::kShowProfilePicker,
                           std::move(show_profile_picker));

  runner.Run();
}

// Tests that when a higher-priority action fails, the lower-priority actions
// don't run.
TEST(IdleActionRunnerTest, OtherActionsDontRunOnFailure) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  FakeActionFactory action_factory;
  ActionRunner runner(&profile, &action_factory);

  base::Value::List actions;
  actions.Append(static_cast<int>(ActionType::kCloseBrowsers));
  actions.Append(static_cast<int>(ActionType::kShowProfilePicker));
  profile.GetPrefs()->SetList(prefs::kIdleTimeoutActions, std::move(actions));

  auto close_browsers =
      std::make_unique<MockAction>(ActionType::kCloseBrowsers);
  auto show_profile_picker =
      std::make_unique<MockAction>(ActionType::kShowProfilePicker);

  // "show_profile_picker" shouldn't run, because "close_browsers" fails.
  testing::InSequence in_sequence;
  EXPECT_CALL(*close_browsers, Run(&profile, _))
      .WillOnce(RunContinuation(false));
  EXPECT_CALL(*show_profile_picker, Run(_, _)).Times(0);

  action_factory.Associate(ActionType::kCloseBrowsers,
                           std::move(close_browsers));
  action_factory.Associate(ActionType::kShowProfilePicker,
                           std::move(show_profile_picker));
  runner.Run();
}

// Tests that it does nothing when the "IdleTimeoutActions" pref is empty.
TEST(IdleActionRunnerTest, DoNothingWithEmptyPref) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  FakeActionFactory action_factory;
  ActionRunner runner(&profile, &action_factory);

  // "IdleTimeoutActions" is deliberately unset.
  auto close_browsers =
      std::make_unique<MockAction>(ActionType::kCloseBrowsers);
  auto show_profile_picker =
      std::make_unique<MockAction>(ActionType::kShowProfilePicker);

  EXPECT_CALL(*close_browsers, Run(_, _)).Times(0);
  EXPECT_CALL(*show_profile_picker, Run(_, _)).Times(0);

  action_factory.Associate(ActionType::kCloseBrowsers,
                           std::move(close_browsers));
  action_factory.Associate(ActionType::kShowProfilePicker,
                           std::move(show_profile_picker));
  runner.Run();
}

// Tests that ActionRunner only runs the actions configured via the
// "IdleTimeoutActions" pref.
TEST(IdleActionRunnerTest, JustCloseBrowsers) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  FakeActionFactory action_factory;
  ActionRunner runner(&profile, &action_factory);

  base::Value::List actions;
  actions.Append(static_cast<int>(ActionType::kCloseBrowsers));
  profile.GetPrefs()->SetList(prefs::kIdleTimeoutActions, std::move(actions));

  auto close_browsers =
      std::make_unique<MockAction>(ActionType::kCloseBrowsers);
  auto show_profile_picker =
      std::make_unique<MockAction>(ActionType::kShowProfilePicker);

  EXPECT_CALL(*close_browsers, Run(&profile, _))
      .WillOnce(RunContinuation(true));
  EXPECT_CALL(*show_profile_picker, Run(_, _)).Times(0);

  action_factory.Associate(ActionType::kCloseBrowsers,
                           std::move(close_browsers));
  action_factory.Associate(ActionType::kShowProfilePicker,
                           std::move(show_profile_picker));
  runner.Run();
}

// Tests that ActionRunner only runs the actions configured via the
// "IdleTimeoutActions" pref.
TEST(IdleActionRunnerTest, JustShowProfilePicker) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  FakeActionFactory action_factory;
  ActionRunner runner(&profile, &action_factory);

  base::Value::List actions;
  actions.Append(static_cast<int>(ActionType::kShowProfilePicker));
  profile.GetPrefs()->SetList(prefs::kIdleTimeoutActions, std::move(actions));

  auto close_browsers =
      std::make_unique<MockAction>(ActionType::kCloseBrowsers);
  auto show_profile_picker =
      std::make_unique<MockAction>(ActionType::kShowProfilePicker);

  EXPECT_CALL(*close_browsers, Run(_, _)).Times(0);
  EXPECT_CALL(*show_profile_picker, Run(&profile, _))
      .WillOnce(RunContinuation(true));

  action_factory.Associate(ActionType::kCloseBrowsers,
                           std::move(close_browsers));
  action_factory.Associate(ActionType::kShowProfilePicker,
                           std::move(show_profile_picker));
  runner.Run();
}

}  // namespace enterprise_idle
