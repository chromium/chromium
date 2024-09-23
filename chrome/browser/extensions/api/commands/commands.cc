// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/commands/commands.h"

#include <memory>
#include <utility>

#include "chrome/browser/extensions/commands/command_service.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/common/api/extension_action/action_info.h"

namespace {

base::Value::Dict CreateCommandValue(const extensions::Command& command,
                                     bool active) {
  base::Value::Dict result;
  result.Set("name", command.command_name());
  result.Set("description", command.description());
  result.Set("shortcut", active ? command.accelerator().GetShortcutText()
                                : std::u16string());
  return result;
}

}  // namespace

ExtensionFunction::ResponseAction GetAllCommandsFunction::Run() {
  base::Value::List command_list;

  extensions::CommandService* command_service =
      extensions::CommandService::Get(browser_context());

  // TODO(crbug.com/40124879): We should be able to check what
  // type of action (if any) the extension has, and just check for
  // that one.
  extensions::Command browser_action;
  bool active = false;
  if (command_service->GetExtensionActionCommand(
          extension_->id(), extensions::ActionInfo::Type::kBrowser,
          extensions::CommandService::ALL, &browser_action, &active)) {
    command_list.Append(CreateCommandValue(browser_action, active));
  }

  extensions::Command action;
  if (command_service->GetExtensionActionCommand(
          extension_->id(), extensions::ActionInfo::Type::kAction,
          extensions::CommandService::ALL, &action, &active)) {
    command_list.Append(CreateCommandValue(action, active));
  }

  extensions::Command page_action;
  if (command_service->GetExtensionActionCommand(
          extension_->id(), extensions::ActionInfo::Type::kPage,
          extensions::CommandService::ALL, &page_action, &active)) {
    command_list.Append(CreateCommandValue(page_action, active));
  }

  extensions::CommandMap named_commands;
  command_service->GetNamedCommands(extension_->id(),
                                    extensions::CommandService::ALL,
                                    extensions::CommandService::ANY_SCOPE,
                                    &named_commands);

  for (extensions::CommandMap::const_iterator iter = named_commands.begin();
       iter != named_commands.end(); ++iter) {
    extensions::Command command = command_service->FindCommandByName(
        extension_->id(), iter->second.command_name());
    ui::Accelerator shortcut_assigned = command.accelerator();
    active = (shortcut_assigned.key_code() != ui::VKEY_UNKNOWN);

    command_list.Append(CreateCommandValue(iter->second, active));
  }

  return RespondNow(WithArguments(std::move(command_list)));
}
