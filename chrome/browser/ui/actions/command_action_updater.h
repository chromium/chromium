// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ACTIONS_COMMAND_ACTION_UPDATER_H_
#define CHROME_BROWSER_UI_ACTIONS_COMMAND_ACTION_UPDATER_H_

#include <optional>
#include <vector>

#include "base/callback_list.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/command_updater.h"
#include "ui/actions/action_id.h"
#include "ui/actions/actions.h"
#include "ui/base/window_open_disposition.h"

class CommandObserver;

namespace actions {
class ActionItem;
}

namespace chrome {

// Manages action mappings and command state tracking for experimental Action
// API. Only used when features::kUseActionsForBrowserCommands is enabled.
class CommandActionUpdater : public CommandUpdater {
 public:
  explicit CommandActionUpdater(actions::ActionItem* root_action_item);
  ~CommandActionUpdater() override;

  // CommandUpdater implementation:
  bool SupportsCommand(int id) const override;
  bool IsCommandEnabled(int id) const override;
  bool ExecuteCommandImpl(
      int id,
      base::TimeTicks time_stamp,
      std::optional<actions::ActionInvocationContext> context) override;
  bool ExecuteCommandWithDispositionImpl(
      int id,
      WindowOpenDisposition disposition,
      base::TimeTicks time_stamp,
      std::optional<actions::ActionInvocationContext> context) override;
  void AddCommandObserver(int id, CommandObserver* observer) override;
  void RemoveCommandObserver(int id, CommandObserver* observer) override;
  void RemoveCommandObserver(CommandObserver* observer) override;
  bool UpdateCommandEnabled(int id, bool state) override;
  void DisableAllCommands() override;
  std::vector<int> GetAllIds() const override;

  std::optional<actions::ActionId> GetActionId(int id) const;

 private:
  actions::ActionItem* FindAction(actions::ActionId action_id) const;
  void ExecuteAction(
      actions::ActionId action_id,
      WindowOpenDisposition disposition,
      std::optional<actions::ActionInvocationContext> context = std::nullopt);
  void OnActionChanged(int id, CommandObserver* observer);

  struct ObserverEntry {
    int id;
    raw_ptr<CommandObserver> observer;
    base::CallbackListSubscription subscription;
  };

  std::vector<ObserverEntry> observer_entries_;
  const raw_ptr<actions::ActionItem> root_action_item_;
};

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_ACTIONS_COMMAND_ACTION_UPDATER_H_
