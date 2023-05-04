// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_IDLE_ACTION_H_
#define CHROME_BROWSER_ENTERPRISE_IDLE_ACTION_H_

#include <memory>
#include <queue>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "chrome/browser/ui/idle_dialog.h"
#include "content/public/browser/browsing_data_remover.h"

class Profile;

namespace enterprise_idle {

// Action types supported by IdleTimeoutActions.
//
// Actions run in order, based on their numerical value. Lower values run first.
// Keep this enum sorted by priority.
enum class ActionType {
  kShowDialog = 0,  // Not an IdleTimeoutAction value. Added as a side-effect.
#if !BUILDFLAG(IS_ANDROID)
  kCloseBrowsers = 1,
  kShowProfilePicker = 2,
#endif  // !BUILDFLAG(IS_ANDROID)
  kClearBrowsingHistory = 3,
  kClearDownloadHistory,
  kClearCookiesAndOtherSiteData,
  kClearCachedImagesAndFiles,
  kClearPasswordSignin,
  kClearAutofill,
  kClearSiteSettings,
  kClearHostedAppData,
  kReloadPages,
  kShowBubble,  // Not an IdleTimeoutAction value. Added as a side-effect.
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

  explicit Action(int priority);
  virtual ~Action();

  Action(const Action&) = delete;
  Action& operator=(const Action&) = delete;
  Action(Action&&) = delete;
  Action& operator=(Action&&) = delete;

  // Runs this action on `profile`, which may be asynchronous. When it's done,
  // runs `continuation` with the result.
  virtual void Run(Profile* profile, Continuation continuation) = 0;

  // Returns true if running this action on `profile` is destructive. If it is,
  // a warning dialog will be shown to inform the user.
  virtual bool ShouldNotifyUserOfPendingDestructiveAction(Profile* profile) = 0;

  int priority() const { return priority_; }

 private:
  const int priority_;
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
  ~ActionFactory();

  // Converts the pref/policy value to a priority_queue<> of actions.
  virtual ActionQueue Build(Profile* profile,
                            const std::vector<ActionType>& action_names);

  void SetBrowsingDataRemoverForTesting(content::BrowsingDataRemover* remover);

 protected:
  friend class base::NoDestructor<ActionFactory>;

  ActionFactory();

  raw_ptr<content::BrowsingDataRemover> browsing_data_remover_for_testing_;
};

IdleDialog::ActionSet ActionsToActionSet(
    const base::flat_set<ActionType>& action_types);

}  // namespace enterprise_idle

#endif  // CHROME_BROWSER_ENTERPRISE_IDLE_ACTION_H_
