// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/commands/command_service.h"

#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#include "base/lazy_instance.h"
#include "base/observer_list.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_commands_global_registry.h"
#include "chrome/browser/extensions/extension_keybinding_registry.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/accelerator_utils.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "extensions/browser/extension_function_registry.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/api/commands/commands_handler.h"
#include "extensions/common/command.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/permissions/permissions_data.h"

namespace extensions {
namespace {

const char kExtension[] = "extension";
const char kCommandName[] = "command_name";
const char kGlobal[] = "global";

// A preference that stores keybinding state associated with extension commands.
const char kCommands[] = "commands";

// Preference key name for saving the extension-suggested key.
const char kSuggestedKey[] = "suggested_key";

// Preference key name for saving whether the extension-suggested key was
// actually assigned.
const char kSuggestedKeyWasAssigned[] = "was_assigned";

std::string GetPlatformKeybindingKeyForAccelerator(
    const ui::Accelerator& accelerator,
    const ExtensionId& extension_id) {
  std::string key = Command::CommandPlatform() + ":" +
                    Command::AcceleratorToString(accelerator);

  // Media keys have a 1-to-many relationship with targets, unlike regular
  // shortcut (1-to-1 relationship). That means two or more extensions can
  // register for the same media key so the extension ID needs to be added to
  // the key to make sure the key is unique.
  if (Command::IsMediaKey(accelerator))
    key += ":" + extension_id;

  return key;
}

bool IsForCurrentPlatform(const std::string& key) {
  return base::StartsWith(key, Command::CommandPlatform() + ":",
                          base::CompareCase::SENSITIVE);
}

// Merge |suggested_key_prefs| into the saved preferences for the extension. We
// merge rather than overwrite to preserve existing was_assigned preferences.
void MergeSuggestedKeyPrefs(const ExtensionId& extension_id,
                            ExtensionPrefs* extension_prefs,
                            base::Value::Dict suggested_key_prefs) {
  const base::Value::Dict* current_prefs =
      extension_prefs->ReadPrefAsDict(extension_id, kCommands);
  if (current_prefs) {
    base::Value::Dict new_prefs = current_prefs->Clone();
    new_prefs.Merge(std::move(suggested_key_prefs));
    suggested_key_prefs = std::move(new_prefs);
  }

  extension_prefs->UpdateExtensionPref(
      extension_id, kCommands, base::Value(std::move(suggested_key_prefs)));
}

}  // namespace

// static
void CommandService::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(
      prefs::kExtensionCommands,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
}

CommandService::CommandService(content::BrowserContext* context)
    : profile_(Profile::FromBrowserContext(context)) {
  extension_registry_observation_.Observe(ExtensionRegistry::Get(profile_));
}

CommandService::~CommandService() {
  for (auto& observer : observers_)
    observer.OnCommandServiceDestroying();
}

static base::LazyInstance<BrowserContextKeyedAPIFactory<CommandService>>::
    DestructorAtExit g_command_service_factory = LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<CommandService>*
CommandService::GetFactoryInstance() {
  return g_command_service_factory.Pointer();
}

// static
CommandService* CommandService::Get(content::BrowserContext* context) {
  return BrowserContextKeyedAPIFactory<CommandService>::Get(context);
}

bool CommandService::GetNamedCommands(const ExtensionId& extension_id,
                                      QueryType type,
                                      CommandScope scope,
                                      CommandMap* command_map) const {
  const Extension* extension =
      GetExtensionInEnabledOrDisabledExtensions(extension_id);
  if (!extension) {
    return false;
  }

  command_map->clear();
  const CommandMap* commands = CommandsInfo::GetNamedCommands(extension);
  if (!commands)
    return false;

  for (const auto& named_command : *commands) {
    // Look up to see if the user has overridden how the command should work.
    Command saved_command =
        FindCommandByName(extension_id, named_command.second.command_name());
    ui::Accelerator shortcut_assigned = saved_command.accelerator();

    if (type == ACTIVE && shortcut_assigned.key_code() == ui::VKEY_UNKNOWN)
      continue;

    Command command = named_command.second;
    if (scope != ANY_SCOPE && ((scope == GLOBAL) != saved_command.global()))
      continue;

    if (shortcut_assigned.key_code() != ui::VKEY_UNKNOWN)
      command.set_accelerator(shortcut_assigned);
    command.set_global(saved_command.global());

    (*command_map)[named_command.second.command_name()] = command;
  }

  return !command_map->empty();
}

bool CommandService::AddKeybindingPref(const ui::Accelerator& accelerator,
                                       const ExtensionId& extension_id,
                                       const std::string& command_name,
                                       bool allow_overrides,
                                       bool global) {
  if (accelerator.key_code() == ui::VKEY_UNKNOWN)
    return false;

  // Nothing needs to be done if the existing command is the same as the desired
  // new one.
  Command existing_command = FindCommandByName(extension_id, command_name);
  if (existing_command.accelerator() == accelerator &&
      existing_command.global() == global)
    return true;

  // Media Keys are allowed to be used by named command only.
  DCHECK(!Command::IsMediaKey(accelerator) ||
         !Command::IsActionRelatedCommand(command_name));

  ScopedDictPrefUpdate updater(profile_->GetPrefs(), prefs::kExtensionCommands);
  base::Value::Dict& bindings = updater.Get();

  std::string key = GetPlatformKeybindingKeyForAccelerator(accelerator,
                                                           extension_id);

  if (bindings.Find(key)) {
    if (!allow_overrides)
      return false;  // Already taken.

    // If the shortcut has been assigned to another command, it should be
    // removed before overriding, so that |ExtensionKeybindingRegistry| can get
    // a chance to do clean-up.
    const base::Value::Dict* item = bindings.FindDict(key);
    const ExtensionId* old_extension_id = item->FindString(kExtension);
    const std::string* old_command_name = item->FindString(kCommandName);
    RemoveKeybindingPrefs(old_extension_id ? *old_extension_id : std::string(),
                          old_command_name ? *old_command_name : std::string());
  }

  // If the command that is taking a new shortcut already has a shortcut, remove
  // it before assigning the new one.
  if (existing_command.accelerator().key_code() != ui::VKEY_UNKNOWN)
    RemoveKeybindingPrefs(extension_id, command_name);

  // Set the keybinding pref.
  base::Value::Dict keybinding;
  keybinding.Set(kExtension, extension_id);
  keybinding.Set(kCommandName, command_name);
  keybinding.Set(kGlobal, global);

  bindings.Set(key, std::move(keybinding));

  // Set the was_assigned pref for the suggested key.
  base::Value::Dict command_keys;
  command_keys.Set(kSuggestedKeyWasAssigned, true);
  base::Value::Dict suggested_key_prefs;
  suggested_key_prefs.Set(command_name, base::Value(std::move(command_keys)));
  MergeSuggestedKeyPrefs(extension_id, ExtensionPrefs::Get(profile_),
                         std::move(suggested_key_prefs));

  // Fetch the newly-updated command, and notify the observers.
  Command command = FindCommandByName(extension_id, command_name);
  for (auto& observer : observers_)
    observer.OnExtensionCommandAdded(extension_id, command);

  return true;
}

void CommandService::OnExtensionWillBeInstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    bool is_update,
    const std::string& old_name) {
  UpdateKeybindings(extension);
}

void CommandService::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    extensions::UninstallReason reason) {
  // Adding a component extensions will only trigger install the first time on a
  // clean profile or on a version increase (see
  // ComponentLoader::AddComponentExtension). It will, however, always trigger
  // an uninstall on removal. See http://crbug.com/458612. Isolate this case and
  // ignore it.
  if (reason == extensions::UNINSTALL_REASON_COMPONENT_REMOVED)
    return;

  RemoveKeybindingPrefs(extension->id(), std::string());
}

void CommandService::UpdateKeybindingPrefs(const ExtensionId& extension_id,
                                           const std::string& command_name,
                                           const std::string& keystroke) {
  Command command = FindCommandByName(extension_id, command_name);

  // The extension command might be assigned another shortcut. Remove that
  // shortcut before proceeding.
  RemoveKeybindingPrefs(extension_id, command_name);

  ui::Accelerator accelerator =
      Command::StringToAccelerator(keystroke, command_name);
  AddKeybindingPref(accelerator, extension_id, command_name,
                    true, command.global());
}

bool CommandService::SetScope(const ExtensionId& extension_id,
                              const std::string& command_name,
                              bool global) {
  Command command = FindCommandByName(extension_id, command_name);
  if (global == command.global())
    return false;

  // Pre-existing shortcuts must be removed before proceeding because the
  // handlers for global and non-global extensions are not one and the same.
  RemoveKeybindingPrefs(extension_id, command_name);
  AddKeybindingPref(command.accelerator(), extension_id,
                    command_name, true, global);
  return true;
}

Command CommandService::FindCommandByName(const ExtensionId& extension_id,
                                          const std::string& command) const {
  const base::Value::Dict& bindings =
      profile_->GetPrefs()->GetDict(prefs::kExtensionCommands);
  for (const auto it : bindings) {
    const ExtensionId* extension = it.second.GetDict().FindString(kExtension);
    if (!extension || *extension != extension_id)
      continue;
    const std::string* command_name =
        it.second.GetDict().FindString(kCommandName);
    if (!command_name || *command_name != command)
      continue;
    // Format stored in Preferences is: "Platform:Shortcut[:ExtensionId]".
    std::string shortcut = it.first;
    if (!IsForCurrentPlatform(shortcut))
      continue;
    std::optional<bool> global = it.second.GetDict().FindBool(kGlobal);

    std::vector<std::string_view> tokens = base::SplitStringPiece(
        shortcut, ":", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    CHECK(tokens.size() >= 2);

    return Command(*command_name, std::u16string(), std::string(tokens[1]),
                   global.value_or(false));
  }

  return Command();
}

void CommandService::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void CommandService::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

const Extension* CommandService::GetExtensionInEnabledOrDisabledExtensions(
    const ExtensionId& extension_id) const {
  const ExtensionSet& enabled_extensions =
      ExtensionRegistry::Get(profile_)->enabled_extensions();
  const Extension* enabled_extension = enabled_extensions.GetByID(extension_id);
  if (enabled_extension) {
    return enabled_extension;
  }
  const ExtensionSet& disabled_extensions =
      ExtensionRegistry::Get(profile_)->disabled_extensions();
  return disabled_extensions.GetByID(extension_id);
}

bool CommandService::IsUpgradeFromMV2ToMV3(
    const Extension* extension,
    const std::string& existing_command_name) const {
  bool browser_or_page_action_command_in_bindings =
      existing_command_name == manifest_values::kBrowserActionCommandEvent ||
      existing_command_name == manifest_values::kPageActionCommandEvent;
  return browser_or_page_action_command_in_bindings &&
         CommandsInfo::GetActionCommand(extension);
}

void CommandService::UpdateKeybindings(const Extension* extension) {
  if (GetExtensionInEnabledOrDisabledExtensions(extension->id())) {
    RemoveRelinquishedKeybindings(extension);
  }
  AssignKeybindings(extension);
  UpdateExtensionSuggestedCommandPrefs(extension);
  RemoveDefunctExtensionSuggestedCommandPrefs(extension);
}

void CommandService::RemoveRelinquishedKeybindings(const Extension* extension) {
  // Remove keybindings if they have been removed by the extension and the user
  // has not modified them.
  CommandMap existing_command_map;
  if (GetNamedCommands(extension->id(),
                       CommandService::ACTIVE,
                       CommandService::REGULAR,
                       &existing_command_map)) {
    const CommandMap* new_command_map =
        CommandsInfo::GetNamedCommands(extension);
    for (CommandMap::const_iterator it = existing_command_map.begin();
         it != existing_command_map.end(); ++it) {
      std::string command_name = it->first;
      if (new_command_map->find(command_name) == new_command_map->end() &&
          !IsCommandShortcutUserModified(extension, command_name)) {
        RemoveKeybindingPrefs(extension->id(), command_name);
      }
    }
  }

  auto remove_overrides_if_unused = [this, extension](ActionInfo::Type type) {
    Command existing_command;
    if (!GetExtensionActionCommand(extension->id(), type,
                                   CommandService::ACTIVE, &existing_command,
                                   nullptr)) {
      // No keybindings to remove.
      return;
    }

    const std::string& existing_command_name = existing_command.command_name();
    bool is_shortcut_user_modified =
        IsCommandShortcutUserModified(extension, existing_command_name);
    bool is_upgrade_from_mv2_to_mv3 =
        IsUpgradeFromMV2ToMV3(extension, existing_command_name);
    if (is_shortcut_user_modified && is_upgrade_from_mv2_to_mv3) {
      // TODO(jlulejian): Could this be an out param to IsUpgradeFromMV2ToMV3?
      const Command* action_command = CommandsInfo::GetActionCommand(extension);
      AddKeybindingPref(existing_command.accelerator(), extension->id(),
                        action_command->command_name(), true,
                        action_command->global());
    } else if (is_shortcut_user_modified) {
      // Don't relinquish user-modified shortcuts otherwise.
      return;
    }

    const Command* new_command = nullptr;
    switch (type) {
      case ActionInfo::Type::kAction:
        new_command = CommandsInfo::GetActionCommand(extension);
        break;
      case ActionInfo::Type::kBrowser:
        new_command = CommandsInfo::GetBrowserActionCommand(extension);
        break;
      case ActionInfo::Type::kPage:
        new_command = CommandsInfo::GetPageActionCommand(extension);
        break;
    }

    // The shortcuts should be removed if there is no command specified in the
    // new extension, or the only command specified is synthesized (i.e.,
    // assigned to ui::VKEY_UNKNOWN), which happens for browser action commands.
    // See CommandsHandler::MaybeSetActionDefault().
    // TODO(devlin): Should this logic apply to ActionInfo::Type::kAction?
    // See https://crbug.com/893373.
    const bool should_relinquish =
        !new_command ||
        (type == ActionInfo::Type::kBrowser &&
         new_command->accelerator().key_code() == ui::VKEY_UNKNOWN);

    if (!should_relinquish)
      return;

    RemoveKeybindingPrefs(extension->id(), existing_command_name);
  };

  // TODO(crbug.com/40124879): Extensions shouldn't be able to specify
  // commands for actions they don't have, so we should just be able to query
  // for a single action type.
  for (ActionInfo::Type type :
       {ActionInfo::Type::kAction, ActionInfo::Type::kBrowser,
        ActionInfo::Type::kPage}) {
    remove_overrides_if_unused(type);
  }
}

void CommandService::AssignKeybindings(const Extension* extension) {
  const CommandMap* commands = CommandsInfo::GetNamedCommands(extension);
  if (!commands)
    return;

  for (const auto& named_command : *commands) {
    const Command command = named_command.second;
    if (CanAutoAssign(command, extension)) {
      AddKeybindingPref(command.accelerator(),
                        extension->id(),
                        command.command_name(),
                        false,  // Overwriting not allowed.
                        command.global());
    }
  }

  const Command* browser_action_command =
      CommandsInfo::GetBrowserActionCommand(extension);
  if (browser_action_command &&
      CanAutoAssign(*browser_action_command, extension)) {
    AddKeybindingPref(browser_action_command->accelerator(),
                      extension->id(),
                      browser_action_command->command_name(),
                      false,   // Overwriting not allowed.
                      false);  // Not global.
  }

  const Command* page_action_command =
      CommandsInfo::GetPageActionCommand(extension);
  if (page_action_command && CanAutoAssign(*page_action_command, extension)) {
    AddKeybindingPref(page_action_command->accelerator(),
                      extension->id(),
                      page_action_command->command_name(),
                      false,   // Overwriting not allowed.
                      false);  // Not global.
  }

  const Command* action_command = CommandsInfo::GetActionCommand(extension);
  if (action_command && CanAutoAssign(*action_command, extension)) {
    AddKeybindingPref(action_command->accelerator(), extension->id(),
                      action_command->command_name(),
                      false,   // Overwriting not allowed.
                      false);  // Not global.
  }
}

bool CommandService::CanAutoAssign(const Command &command,
                                   const Extension* extension) {
  // Extensions are allowed to auto-assign updated keys if the user has not
  // changed from the previous value.
  if (IsCommandShortcutUserModified(extension, command.command_name()))
    return false;

  // Media Keys are non-exclusive, so allow auto-assigning them.
  if (Command::IsMediaKey(command.accelerator()))
    return true;

  if (command.global()) {
    if (Command::IsActionRelatedCommand(command.command_name()))
      return false;  // Browser and page actions are not global in nature.

    if (extension->permissions_data()->HasAPIPermission(
            mojom::APIPermissionID::kCommandsAccessibility))
      return true;

    // Global shortcuts are restricted to (Ctrl|Command)+Shift+[0-9].
#if BUILDFLAG(IS_MAC)
    if (!command.accelerator().IsCmdDown())
      return false;
#else
    if (!command.accelerator().IsCtrlDown())
      return false;
#endif
    if (!command.accelerator().IsShiftDown())
      return false;
    return (command.accelerator().key_code() >= ui::VKEY_0 &&
            command.accelerator().key_code() <= ui::VKEY_9);
  }

  // Not a global command, check if the command is a Chrome shortcut.
  return !chrome::IsChromeAccelerator(command.accelerator());
}

void CommandService::UpdateExtensionSuggestedCommandPrefs(
    const Extension* extension) {
  base::Value::Dict suggested_key_prefs;

  const CommandMap* commands = CommandsInfo::GetNamedCommands(extension);
  if (commands) {
    for (const auto& named_command : *commands) {
      const Command command = named_command.second;
      base::Value::Dict command_keys;
      command_keys.Set(kSuggestedKey,
                       Command::AcceleratorToString(command.accelerator()));
      suggested_key_prefs.Set(command.command_name(), std::move(command_keys));
    }
  }

  const Command* browser_action_command =
      CommandsInfo::GetBrowserActionCommand(extension);
  // The browser action command may be defaulted to an unassigned accelerator if
  // a browser action is specified by the extension but a keybinding is not
  // declared. See CommandsHandler::MaybeSetActionDefault.
  if (browser_action_command &&
      browser_action_command->accelerator().key_code() != ui::VKEY_UNKNOWN) {
    base::Value::Dict command_keys;
    command_keys.Set(kSuggestedKey, Command::AcceleratorToString(
                                        browser_action_command->accelerator()));
    suggested_key_prefs.Set(browser_action_command->command_name(),
                            std::move(command_keys));
  }

  const Command* page_action_command =
      CommandsInfo::GetPageActionCommand(extension);
  if (page_action_command) {
    base::Value::Dict command_keys;
    command_keys.Set(kSuggestedKey, Command::AcceleratorToString(
                                        page_action_command->accelerator()));
    suggested_key_prefs.Set(page_action_command->command_name(),
                            std::move(command_keys));
  }

  // Merge into current prefs, if present.
  MergeSuggestedKeyPrefs(extension->id(), ExtensionPrefs::Get(profile_),
                         std::move(suggested_key_prefs));
}

void CommandService::RemoveDefunctExtensionSuggestedCommandPrefs(
    const Extension* extension) {
  ExtensionPrefs* extension_prefs = ExtensionPrefs::Get(profile_);
  const base::Value::Dict* current_prefs =
      extension_prefs->ReadPrefAsDict(extension->id(), kCommands);

  if (current_prefs) {
    base::Value::Dict suggested_key_prefs = current_prefs->Clone();

    const CommandMap* named_commands =
        CommandsInfo::GetNamedCommands(extension);

    const Command* browser_action_command =
        CommandsInfo::GetBrowserActionCommand(extension);
    for (const auto [key, _] : *current_prefs) {
      if (key == manifest_values::kBrowserActionCommandEvent) {
        // The browser action command may be defaulted to an unassigned
        // accelerator if a browser action is specified by the extension but a
        // keybinding is not declared. See
        // CommandsHandler::MaybeSetActionDefault.
        if (!browser_action_command ||
            browser_action_command->accelerator().key_code() ==
                ui::VKEY_UNKNOWN) {
          suggested_key_prefs.Remove(key);
        }
      } else if (key == manifest_values::kPageActionCommandEvent) {
        if (!CommandsInfo::GetPageActionCommand(extension))
          suggested_key_prefs.Remove(key);
      } else if (key == manifest_values::kActionCommandEvent) {
        if (!CommandsInfo::GetActionCommand(extension))
          suggested_key_prefs.Remove(key);
      } else if (named_commands) {
        if (named_commands->find(key) == named_commands->end())
          suggested_key_prefs.Remove(key);
      }
    }

    extension_prefs->UpdateExtensionPref(
        extension->id(), kCommands,
        base::Value(std::move(suggested_key_prefs)));
  }
}

bool CommandService::IsCommandShortcutUserModified(
    const Extension* extension,
    const std::string& command_name) {
  // Get the previous suggested key, if any.
  ui::Accelerator suggested_key;
  std::optional<bool> suggested_key_was_assigned;
  ExtensionPrefs* extension_prefs = ExtensionPrefs::Get(profile_);
  const base::Value::Dict* commands_prefs =
      extension_prefs->ReadPrefAsDict(extension->id(), kCommands);
  if (commands_prefs) {
    const base::Value::Dict* suggested_key_prefs =
        commands_prefs->FindDict(command_name);
    if (suggested_key_prefs) {
      const std::string* suggested_key_string =
          suggested_key_prefs->FindString(kSuggestedKey);
      if (suggested_key_string) {
        suggested_key =
            Command::StringToAccelerator(*suggested_key_string, command_name);
      }
      suggested_key_was_assigned =
          suggested_key_prefs->FindBool(kSuggestedKeyWasAssigned);
    }
  }

  // Get the active shortcut from the prefs, if any.
  Command active_command = FindCommandByName(extension->id(), command_name);

  return suggested_key_was_assigned.value_or(false)
             ? active_command.accelerator() != suggested_key
             : active_command.accelerator().key_code() != ui::VKEY_UNKNOWN;
}

void CommandService::RemoveKeybindingPrefs(const ExtensionId& extension_id,
                                           const std::string& command_name) {
  ScopedDictPrefUpdate updater(profile_->GetPrefs(), prefs::kExtensionCommands);
  base::Value::Dict& bindings = updater.Get();

  typedef std::vector<std::string> KeysToRemove;
  KeysToRemove keys_to_remove;
  std::vector<Command> removed_commands;
  for (const auto it : bindings) {
    // Removal of keybinding preference should be limited to current platform.
    if (!IsForCurrentPlatform(it.first))
      continue;

    const base::Value::Dict& dict = it.second.GetDict();
    const ExtensionId* extension = dict.FindString(kExtension);

    if (extension && *extension == extension_id) {
      // If |command_name| is specified, delete only that command. Otherwise,
      // delete all commands.
      const std::string* command = dict.FindString(kCommandName);
      if (command && !command_name.empty() && command_name != *command)
        continue;

      removed_commands.push_back(FindCommandByName(extension_id, *command));
      keys_to_remove.push_back(it.first);
    }
  }

  for (KeysToRemove::const_iterator it = keys_to_remove.begin();
       it != keys_to_remove.end(); ++it) {
    std::string key = *it;
    bindings.Remove(key);
  }

  for (const Command& removed_command : removed_commands) {
    for (auto& observer : observers_)
      observer.OnExtensionCommandRemoved(extension_id, removed_command);
  }
}

bool CommandService::GetExtensionActionCommand(const ExtensionId& extension_id,
                                               ActionInfo::Type action_type,
                                               QueryType query_type,
                                               Command* command,
                                               bool* active) const {
  const Extension* extension =
      GetExtensionInEnabledOrDisabledExtensions(extension_id);
  if (!extension) {
    return false;
  }

  if (active)
    *active = false;

  const Command* requested_command = nullptr;
  switch (action_type) {
    case ActionInfo::Type::kBrowser:
      requested_command = CommandsInfo::GetBrowserActionCommand(extension);
      break;
    case ActionInfo::Type::kPage:
      requested_command = CommandsInfo::GetPageActionCommand(extension);
      break;
    case ActionInfo::Type::kAction:
      requested_command = CommandsInfo::GetActionCommand(extension);
      break;
  }
  if (!requested_command)
    return false;

  // Look up to see if the user has overridden how the command should work.
  Command saved_command =
      FindCommandByName(extension_id, requested_command->command_name());
  ui::Accelerator shortcut_assigned = saved_command.accelerator();

  if (active)
    *active = (shortcut_assigned.key_code() != ui::VKEY_UNKNOWN);

  if (query_type == ACTIVE && shortcut_assigned.key_code() == ui::VKEY_UNKNOWN)
    return false;

  *command = *requested_command;
  if (shortcut_assigned.key_code() != ui::VKEY_UNKNOWN)
    command->set_accelerator(shortcut_assigned);

  return true;
}

template <>
void BrowserContextKeyedAPIFactory<
    CommandService>::DeclareFactoryDependencies() {
  DependsOn(ExtensionCommandsGlobalRegistry::GetFactoryInstance());
}

}  // namespace extensions
