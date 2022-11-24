// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_IDLE_ACTION_H_
#define CHROME_BROWSER_ENTERPRISE_IDLE_ACTION_H_

#include <queue>
#include <vector>

#include "base/callback.h"
#include "base/containers/flat_set.h"
#include "base/containers/span.h"

class Profile;

namespace enterprise_idle {

// Action types supported by IdleTimeoutActions.
//
// Actions run in order, based on their numerical value. Lower values run first.
// Keep this enum sorted by priority.
enum class ActionType {
  kCloseBrowsers = 0,
  kShowProfilePicker = 1,
};

// A mapping of names to enums, for the ConfigurationPolicyHandler to make
// conversions.
struct ActionTypeMapEntry {
  const char* name;
  ActionType action_type;
};
extern const ActionTypeMapEntry kActionTypeMap[];
extern const size_t kActionTypeMapSize;

// An action that should Run() when a given event happens. See *Actions
// policies, e.g. IdleTimeoutActions.
class Action {
 public:
  using Continuation = base::OnceCallback<void(bool succeeded)>;

  explicit Action(ActionType action_type);
  virtual ~Action();

  Action(const Action&) = delete;
  Action& operator=(const Action&) = delete;
  Action(Action&&) = delete;
  Action& operator=(Action&&) = delete;

  virtual void Run(Profile* profile, Continuation continuation) = 0;

  unsigned priority() const { return static_cast<unsigned>(action_type_); }

 private:
  const ActionType action_type_;
};

// A singleton factory that takes a list of `ActionType` and converts it to a
// `priority_queue<Action>`. See Build().
class ActionFactory {
 public:
  struct CompareActionsByPriority {
    bool operator()(const std::unique_ptr<Action>& a,
                    const std::unique_ptr<Action>& b) const;
  };

  using ActionQueue = std::priority_queue<std::unique_ptr<Action>,
                                          std::vector<std::unique_ptr<Action>>,
                                          CompareActionsByPriority>;

  static ActionFactory* GetInstance();

  ActionFactory(const ActionFactory&) = delete;
  ActionFactory& operator=(const ActionFactory&) = delete;
  ActionFactory(ActionFactory&&) = delete;
  ActionFactory& operator=(ActionFactory&&) = delete;

  // Converts the pref/policy value to a priority_queue<> of actions.
  virtual ActionQueue Build(const std::vector<ActionType>& action_names);

 protected:
  ActionFactory();
};

}  // namespace enterprise_idle

#endif  // CHROME_BROWSER_ENTERPRISE_IDLE_ACTION_H_
