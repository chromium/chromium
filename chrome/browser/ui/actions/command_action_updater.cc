// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/actions/command_action_updater.h"

#include <utility>

#include "base/containers/flat_map.h"
#include "base/no_destructor.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/command_observer.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/actions/chrome_action_properties.h"
#include "ui/actions/actions.h"

namespace chrome {
namespace {

#define MAP_ACTION_E1(action)
#define MAP_ACTION_E2(action, idc) {idc, action},
#define MAP_ACTION_E3(action, idc, scope) {idc, scope::action},
#define MAP_ACTION_E4(action, idc, val, scope) {idc, scope::action},

#define GET_MAP_ACTION_E(_1, _2, _3, _4, macro_name, ...) macro_name
#define E(...)                                                               \
  GET_MAP_ACTION_E(__VA_ARGS__, MAP_ACTION_E4, MAP_ACTION_E3, MAP_ACTION_E2, \
                   MAP_ACTION_E1)(__VA_ARGS__)

// Maps Browser Command IDs (IDC_*) to their corresponding declarative Action
// IDs.
//
// MIGRATION GUIDE:
// -------------------------------
// To migrate a legacy browser command to the modern Action framework:
//
// 1. Define your new Action in `chrome/browser/ui/actions/chrome_action_id.h`.
// 2. Initialize your Action Item in `BrowserActions::Initialize...`.
// 3. Associate your legacy `IDC_*` command ID directly in the macro entry
//    (e.g. `E(kActionFoo, IDC_FOO)`).
//
// Once registered there, `CommandActionUpdater` automatically intercepts state
// updates (`UpdateCommandEnabled`) and executions for your command and routes
// them into the declarative Action framework.
const base::flat_map<int, actions::ActionId>& GetCommandIdToActionIdMap() {
  static const base::NoDestructor<base::flat_map<int, actions::ActionId>> kMap(
      [] {
        std::vector<std::pair<int, actions::ActionId>> entries = {
            CHROME_ACTION_IDS SIDE_PANEL_ACTION_IDS TOOLBAR_PINNABLE_ACTION_IDS
                SUBMENU_ACTION_IDS};
        base::flat_map<int, actions::ActionId> map;
        map.reserve(entries.size());
        for (const auto& [idc, action_id] : entries) {
          map.insert({idc, action_id});
        }
        return map;
      }());
  return *kMap;
}

#undef E
#undef GET_MAP_ACTION_E
#undef MAP_ACTION_E1
#undef MAP_ACTION_E2
#undef MAP_ACTION_E3
#undef MAP_ACTION_E4

}  // namespace

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

std::optional<actions::ActionId> CommandActionUpdater::GetActionId(
    int id) const {
  const auto& map = GetCommandIdToActionIdMap();
  auto it = map.find(id);
  return it != map.end() ? std::make_optional(it->second) : std::nullopt;
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
