// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/actions/command_action_updater.h"

#include <utility>

#include "chrome/browser/command_observer.h"
#include "chrome/browser/ui/actions/chrome_action_properties.h"
#include "chrome/browser/ui/actions/command_id_to_action_id.h"
#include "ui/actions/actions.h"
#include "ui/base/window_open_disposition.h"

namespace chrome {

// See `GetCommandIdToActionIdMap()` in command_id_to_action_id.h for how to
// migrate a legacy browser command to the Action framework.

CommandActionUpdater::CommandActionUpdater(
    actions::ActionItem* root_action_item)
    : root_action_item_(root_action_item) {}

CommandActionUpdater::~CommandActionUpdater() = default;

bool CommandActionUpdater::SupportsCommand(int id) const {
  return GetCommandIdToActionIdMap().contains(id);
}

bool CommandActionUpdater::IsCommandEnabled(int id) const {
  if (auto action_id = GetActionId(id)) {
    if (auto* const action = FindAction(*action_id)) {
      return action->GetEnabled();
    }
  }
  return false;
}

bool CommandActionUpdater::ExecuteCommandWithDispositionAndContext(
    int id,
    WindowOpenDisposition disposition,
    std::optional<actions::ActionInvocationContext> context,
    base::TimeTicks time_stamp) {
  if (SupportsCommand(id) && IsCommandEnabled(id)) {
    if (auto action_id = GetActionId(id)) {
      ExecuteAction(*action_id, disposition, std::move(context));
      return true;
    }
  }
  return false;
}

void CommandActionUpdater::AddCommandObserver(int id,
                                              CommandObserver* observer) {
  if (auto action_id = GetActionId(id)) {
    if (auto* const action = FindAction(*action_id)) {
      base::CallbackListSubscription sub = action->AddActionChangedCallback(
          base::BindRepeating(&CommandActionUpdater::OnActionChanged,
                              base::Unretained(this), id, observer));
      observer_entries_.push_back({id, observer, std::move(sub)});
    }
  }
}

void CommandActionUpdater::RemoveCommandObserver(int id,
                                                 CommandObserver* observer) {
  std::erase_if(observer_entries_, [&](const auto& entry) {
    return entry.id == id && entry.observer == observer;
  });
}

void CommandActionUpdater::RemoveCommandObserver(CommandObserver* observer) {
  std::erase_if(observer_entries_,
                [&](const auto& entry) { return entry.observer == observer; });
}

bool CommandActionUpdater::UpdateCommandEnabled(int id, bool state) {
  if (auto action_id = GetActionId(id)) {
    if (auto* const action = FindAction(*action_id)) {
      action->SetEnabled(state);
      return true;
    }
  }
  return false;
}

void CommandActionUpdater::DisableAllCommands() {
  for (const auto& [idc, action_id] : GetCommandIdToActionIdMap()) {
    if (auto* const action = FindAction(action_id)) {
      action->SetEnabled(false);
    }
  }
}

std::vector<int> CommandActionUpdater::GetAllIds() const {
  std::vector<int> result;
  const auto& map = GetCommandIdToActionIdMap();
  result.reserve(map.size());
  for (const auto& [idc, action_id] : map) {
    result.push_back(idc);
  }
  return result;
}

// static
std::optional<actions::ActionId> CommandActionUpdater::GetActionId(int id) {
  return GetActionIdForCommandId(id);
}

actions::ActionItem* CommandActionUpdater::FindAction(
    actions::ActionId action_id) const {
  if (!root_action_item_) {
    return nullptr;
  }
  return actions::ActionManager::Get().FindAction(action_id, root_action_item_);
}

void CommandActionUpdater::ExecuteAction(
    actions::ActionId action_id,
    WindowOpenDisposition disposition,
    std::optional<actions::ActionInvocationContext> context) {
  if (auto* const action = FindAction(action_id)) {
    actions::ActionInvocationContext invocation_context =
        context.has_value() ? std::move(*context)
                            : actions::ActionInvocationContext();
    invocation_context.SetProperty(kDispositionKey, disposition);
    action->InvokeAction(std::move(invocation_context));
  }
}

void CommandActionUpdater::OnActionChanged(int id, CommandObserver* observer) {
  observer->EnabledStateChangedForCommand(id, IsCommandEnabled(id));
}

}  // namespace chrome
