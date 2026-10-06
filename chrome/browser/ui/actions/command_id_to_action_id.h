// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ACTIONS_COMMAND_ID_TO_ACTION_ID_H_
#define CHROME_BROWSER_UI_ACTIONS_COMMAND_ID_TO_ACTION_ID_H_

#include <optional>

#include "base/containers/flat_map.h"
#include "ui/actions/action_id.h"

namespace chrome {

// Maps Browser Command IDs (IDC_*) to their corresponding declarative Action
// IDs, as declared in `chrome/browser/ui/actions/chrome_action_id.h`.
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
const base::flat_map<int, actions::ActionId>& GetCommandIdToActionIdMap();

// Returns the Action ID associated with `command_id`, or std::nullopt if the
// command has no associated action.
std::optional<actions::ActionId> GetActionIdForCommandId(int command_id);

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_ACTIONS_COMMAND_ID_TO_ACTION_ID_H_
