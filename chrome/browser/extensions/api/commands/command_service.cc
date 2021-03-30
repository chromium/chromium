// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/commands/command_service.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/lazy_instance.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/commands/commands.h"
#include "chrome/browser/extensions/extension_commands_global_registry.h"
#include "chrome/browser/extensions/extension_keybinding_registry.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/accelerator_utils.h"
#include "chrome/common/extensions/api/commands/commands_handler.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "extensions/browser/extension_function_registry.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/notification_types.h"
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
    const ui::Accelerator& accelerator, const std::string& extension_id) {
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
void MergeSuggestedKeyPrefs(
    const std::string& extension_id,
    ExtensionPrefs* extension_prefs,
    std::unique_ptr<base::DictionaryValue> suggested_key_prefs) {
  const base::DictionaryValue* current_prefs;
  if (extension_prefs->ReadPrefAsDictionary(extension_id,
                                            kCommands,
                                            &current_prefs)) {
    std::unique_ptr<base::DictionaryValue> new_prefs(current_prefs->DeepCopy());
    new_prefs->MergeDictionary(suggested_key_prefs.get());
    suggested_key_prefs = std::move(new_prefs);
  }

  extension_prefs->UpdateExtensionPref(extension_id, kCommands,
                                       std::move(suggested_key_prefs));
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
  ExtensionFunctionRegistry::GetInstance()
      .RegisterFunction<GetAllCommandsFunction>();

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

bool CommandService::GetNamedCommands(const std::string& extension_id,
                                      QueryType type,
                                      CommandScope scope,
                                      CommandMap* command_map) const {
  const ExtensionSet& extensions =
      ExtensionRegistry::Get(profile_)->enabled_extensions();
  const Extension* extension = extensions.GetByID(extension_id);
  CHECK(extension);

  command_map->clear();
  const CommandMap* commands = CommandsInfo::GetNamedCommands(extension);
  if (!commands)
    return false;

  for (auto iter = commands->cbegin(); iter != commands->cend(); ++iter) {
    // Look up to see if the user has overridden how the command should work.
    Command saved_command =
        FindCommandByName(extension_id, iter->second.command_name());
    ui::Accelerator shortcut_assigned = saved_command.accelerator();

    if (type == ACTIVE && shortcut_assigned.key_code() == ui::VKEY_UNKNOWN)
      continue;

    Command command = iter->second;
    if (scope != ANY_SCOPE && ((scope == GLOBAL) != saved_command.global()))
      continue;

    if (shortcut_assigned.key_code() != ui::VKEY_UNKNOWN)
      command.set_accelerator(shortcut_assigned);
    command.set_global(saved_command.global());

    (*command_map)[iter->second.command_name()] = command;
  }

  return !command_map->empty();
}

bool CommandService::AddKeybindingPref(
    const ui::Accelerator& accelerator,
    const std::string& extension_id,
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
         (command_name != manifest_values::kPageActionCommandEvent &&
          command_name != manifest_values::kBrowserActionCommandEvent &&
          command_name != manifest_values::kActionCommandEvent));

  DictionaryPrefUpdate updater(profile_->GetPrefs(),
                               prefs::kExtensionCommands);
  base::DictionaryValue* bindings = updater.Get();

  std::string key = GetPlatformKeybindingKeyForAccelerator(accelerator,
                                                           extension_id);

  if (bindings->HasKey(key)) {
    if (!allow_overrides)
      return false;  // Already taken.

    // If the shortcut has been assigned to another command, it should be
    // removed before overriding, so that |ExtensionKeybindingRegistry| can get
    // a chance to do clean-up.
    const base::DictionaryValue* item = NULL;
    bindings->GetDictionary(key, &item);
    std::string old_extension_id;
    std::string old_command_name;
    item->GetString(kExtension, &old_extension_id);
    item->GetString(kCommandName, &old_command_name);
    RemoveKeybindingPrefs(old_extension_id, old_command_name);
  }

  // If the command that is taking a new shortcut already has a shortcut, remove
  // it before assigning the new one.
  if (existing_command.accelerator().key_code() != ui::VKEY_UNKNOWN)
    RemoveKeybindingPrefs(extension_id, command_name);

  // Set the keybinding pref.
  auto keybinding = std::make_unique<base::DictionaryValue>();
  keybinding->SetString(kExtension, extension_id);
  keybinding->SetString(kCommandName, command_name);
  keybinding->SetBoolean(kGlobal, global);

  bindings->Set(key, std::move(keybinding));

  // Set the was_assigned pref for the suggested key.
  std::unique_ptr<base::DictionaryValue> command_keys(
      new base::DictionaryValue);
  command_keys->SetBoolean(kSuggestedKeyWasAssigned, true);
  std::unique_ptr<base::DictionaryValue> suggested_key_prefs(
      new base::DictionaryValue);
  suggested_key_prefs->Set(command_name, std::move(command_keys));
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

void CommandService::UpdateKeybindingPrefs(const std::string& extension_id,
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

bool CommandService::SetScope(const std::string& extension_id,
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

Command CommandService::FindCommandByName(const std::string& extension_id,
                                          const std::string& command) const {
  const base::DictionaryValue* bindings =
      profile_->GetPrefs()->GetDictionary(prefs::kExtensionCommands);
  for (base::DictionaryValue::Iterator it(*bindings); !it.IsAtEnd();
       it.Advance()) {
    const base::DictionaryValue* item = NULL;
    it.value().GetAsDictionary(&item);

    std::string extension;
    item->GetString(kExtension, &extension);
    if (extension != extension_id)
      continue;
    std::string command_name;
    item->GetString(kCommandName, &command_name);
    if (command != command_name)
      continue;
    // Format stored in Preferences is: "Platform:Shortcut[:ExtensionId]".
    std::string shortcut = it.key();
    if (!IsForCurrentPlatform(shortcut))
      continue;
    bool global = false;
    item->GetBoolean(kGlobal, &global);

    std::vector<base::StringPiece> tokens = base::SplitStringPiece(
        shortcut, ":", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    CHECK(tokens.size() >= 2);

    return Command(command_name, std::u16string(), tokens[1].as_string(),
                   global);
  }

  return Command();
}

void CommandService::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void CommandService::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void CommandService::UpdateKeybindings(const Extension* extension) {
  const ExtensionSet& extensions =
      ExtensionRegistry::Get(profile_)->enabled_extensions();
  // The extension is not added to the profile by this point on first install,
  // so don't try to check for existing keybindings.
  if (extensions.GetByID(extension->id()))
    RemoveRelinquishedKeybindings(extension);
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

    if (IsCommandShortcutUserModified(extension,
                                      existing_command.command_name())) {
      // Don't relinquish user-modified shortcuts.
      return;
    }

    const Command* new_command = nullptr;
    switch (type) {
      case ActionInfo::TYPE_ACTION:
        new_command = CommandsInfo::GetActionCommand(extension);
        break;
      case ActionInfo::TYPE_BROWSER:
        new_command = CommandsInfo::GetBrowserActionCommand(extension);
        break;
      case ActionInfo::TYPE_PAGE:
        new_command = CommandsInfo::GetPageActionCommand(extension);
        break;
    }

    // The shortcuts should be removed if there is no command specified in the
    // new extension, or the only command specified is synthesized (i.e.,
    // assigned to ui::VKEY_UNKNOWN), which happens for browser action commands.
    // See CommandsHandler::MaybeSetBrowserActionDefault().
    // TODO(devlin): Should this logic apply to ActionInfo::TYPE_ACTION?
    // See https://crbug.com/893373.
    const bool should_relinquish =
        !new_command ||
        (type == ActionInfo::TYPE_BROWSER &&
         new_command->accelerator().key_code() == ui::VKEY_UNKNOWN);

    if (!should_relinquish)
      return;

    RemoveKeybindingPrefs(extension->id(), existing_command.command_name());
  };

  // TODO(https://crbug.com/1067130): Extensions shouldn't be able to specify
  // commands for actions they don't have, so we should just be able to query
  // for a single action type.
  for (ActionInfo::Type type :
       {ActionInfo::TYPE_ACTION, ActionInfo::TYPE_BROWSER,
        ActionInfo::TYPE_PAGE}) {
    remove_overrides_if_unused(type);
  }
}

void CommandService::AssignKeybindings(const Extension* extension) {
  const CommandMap* commands = CommandsInfo::GetNamedCommands(extension);
  if (!commands)
    return;

  for (auto iter = commands->cbegin(); iter != commands->cend(); ++iter) {
    const Command command = iter->second;
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
    if (command.command_name() == manifest_values::kBrowserActionCommandEvent ||
        command.command_name() == manifest_values::kPageActionCommandEvent ||
        command.command_name() == manifest_values::kActionCommandEvent)
      return false;  // Browser and page actions are not global in nature.

    if (extension->permissions_data()->HasAPIPermission(
            mojom::APIPermissionID::kCommandsAccessibility))
      return true;

    // Global shortcuts are restricted to (Ctrl|Command)+Shift+[0-9].
#if defined(OS_MAC)
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
  std::unique_ptr<base::DictionaryValue> suggested_key_prefs(
      new base::DictionaryValue);

  const CommandMap* commands = CommandsInfo::GetNamedCommands(extension);
  if (commands) {
    for (auto iter = commands->cbegin(); iter != commands->cend(); ++iter) {
      const Command command = iter->second;
      std::unique_ptr<base::DictionaryValue> command_keys(
          new base::DictionaryValue);
      command_keys->SetString(
          kSuggestedKey,
          Command::AcceleratorToString(command.accelerator()));
      suggested_key_prefs->Set(command.command_name(), std::move(command_keys));
    }
  }

  const Command* browser_action_command =
      CommandsInfo::GetBrowserActionCommand(extension);
  // The browser action command may be defaulted to an unassigned accelerator if
  // a browser action is specified by the extension but a keybinding is not
  // declared. See CommandsHandler::MaybeSetBrowserActionDefault.
  if (browser_action_command &&
      browser_action_command->accelerator().key_code() != ui::VKEY_UNKNOWN) {
    std::unique_ptr<base::DictionaryValue> command_keys(
        new base::DictionaryValue);
    command_keys->SetString(
        kSuggestedKey,
        Command::AcceleratorToString(browser_action_command->accelerator()));
    suggested_key_prefs->Set(browser_action_command->command_name(),
                             std::move(command_keys));
  }

  const Command* page_action_command =
      CommandsInfo::GetPageActionCommand(extension);
  if (page_action_command) {
    std::unique_ptr<base::DictionaryValue> command_keys(
        new base::DictionaryValue);
    command_keys->SetString(
        kSuggestedKey,
        Command::AcceleratorToString(page_action_command->accelerator()));
    suggested_key_prefs->Set(page_action_command->command_name(),
                             std::move(command_keys));
  }

  // Merge into current prefs, if present.
  MergeSuggestedKeyPrefs(extension->id(), ExtensionPrefs::Get(profile_),
                         std::move(suggested_key_prefs));
}

void CommandService::RemoveDefunctExtensionSuggestedCommandPrefs(
    const Extension* extension) {
  ExtensionPrefs* extension_prefs = ExtensionPrefs::Get(profile_);
  const base::DictionaryValue* current_prefs = NULL;
  extension_prefs->ReadPrefAsDictionary(extension->id(),
                                        kCommands,
                                        &current_prefs);

  if (current_prefs) {
    std::unique_ptr<base::DictionaryValue> suggested_key_prefs(
        current_prefs->DeepCopy());
    const CommandMap* named_commands =
        CommandsInfo::GetNamedCommands(extension);

    const Command* browser_action_command =
        CommandsInfo::GetBrowserActionCommand(extension);
    for (base::DictionaryValue::Iterator it(*current_prefs);
         !it.IsAtEnd(); it.Advance()) {
      if (it.key() == manifest_values::kBrowserActionCommandEvent) {
        // The browser action command may be defaulted to an unassigned
        // accelerator if a browser action is specified by the extension but a
        // keybinding is not declared. See
        // CommandsHandler::MaybeSetBrowserActionDefault.
        if (!browser_action_command ||
            browser_action_command->accelerator().key_code() ==
                ui::VKEY_UNKNOWN) {
          suggested_key_prefs->Remove(it.key(), NULL);
        }
      } else if (it.key() == manifest_values::kPageActionCommandEvent) {
        if (!CommandsInfo::GetPageActionCommand(extension))
          suggested_key_prefs->Remove(it.key(), NULL);
      } else if (it.key() == manifest_values::kActionCommandEvent) {
        if (!CommandsInfo::GetActionCommand(extension))
          suggested_key_prefs->Remove(it.key(), nullptr);
      } else if (named_commands) {
        if (named_commands->find(it.key()) == named_commands->end())
          suggested_key_prefs->Remove(it.key(), NULL);
      }
    }

    extension_prefs->UpdateExtensionPref(extension->id(), kCommands,
                                         std::move(suggested_key_prefs));
  }
}

bool CommandService::IsCommandShortcutUserModified(
    const Extension* extension,
    const std::string& command_name) {
  // Get the previous suggested key, if any.
  ui::Accelerator suggested_key;
  bool suggested_key_was_assigned = false;
  ExtensionPrefs* extension_prefs = ExtensionPrefs::Get(profile_);
  const base::DictionaryValue* commands_prefs = NULL;
  const base::DictionaryValue* suggested_key_prefs = NULL;
  if (extension_prefs->ReadPrefAsDictionary(extension->id(),
                                            kCommands,
                                            &commands_prefs) &&
      commands_prefs->GetDictionary(command_name, &suggested_key_prefs)) {
    std::string suggested_key_string;
    if (suggested_key_prefs->GetString(kSuggestedKey, &suggested_key_string)) {
      suggested_key = Command::StringToAccelerator(suggested_key_string,
                                                   command_name);
    }

    suggested_key_prefs->GetBoolean(kSuggestedKeyWasAssigned,
                                    &suggested_key_was_assigned);
  }

  // Get the active shortcut from the prefs, if any.
  Command active_command = FindCommandByName(extension->id(), command_name);

  return suggested_key_was_assigned ?
      active_command.accelerator() != suggested_key :
      active_command.accelerator().key_code() != ui::VKEY_UNKNOWN;
}

void CommandService::RemoveKeybindingPrefs(const std::string& extension_id,
                                           const std::string& command_name) {
  DictionaryPrefUpdate updater(profile_->GetPrefs(),
                               prefs::kExtensionCommands);
  base::DictionaryValue* bindings = updater.Get();

  typedef std::vector<std::string> KeysToRemove;
  KeysToRemove keys_to_remove;
  std::vector<Command> removed_commands;
  for (base::DictionaryValue::Iterator it(*bindings); !it.IsAtEnd();
       it.Advance()) {
    // Removal of keybinding preference should be limited to current platform.
    if (!IsForCurrentPlatform(it.key()))
      continue;

    const base::DictionaryValue* item = NULL;
    it.value().GetAsDictionary(&item);

    std::string extension;
    item->GetString(kExtension, &extension);

    if (extension == extension_id) {
      // If |command_name| is specified, delete only that command. Otherwise,
      // delete all commands.
      std::string command;
      item->GetString(kCommandName, &command);
      if (!command_name.empty() && command_name != command)
        continue;

      removed_commands.push_back(FindCommandByName(extension_id, command));
      keys_to_remove.push_back(it.key());
    }
  }

  for (KeysToRemove::const_iterator it = keys_to_remove.begin();
       it != keys_to_remove.end(); ++it) {
    std::string key = *it;
    bindings->Remove(key, NULL);
  }

  for (const Command& removed_command : removed_commands) {
    for (auto& observer : observers_)
      observer.OnExtensionCommandRemoved(extension_id, removed_command);
  }
}

bool CommandService::GetExtensionActionCommand(const std::string& extension_id,
                                               ActionInfo::Type action_type,
                                               QueryType query_type,
                                               Command* command,
                                               bool* active) const {
  const ExtensionSet& extensions =
      ExtensionRegistry::Get(profile_)->enabled_extensions();
  const Extension* extension = extensions.GetByID(extension_id);
  CHECK(extension);

  if (active)
    *active = false;

  const Command* requested_command = NULL;
  switch (action_type) {
    case ActionInfo::TYPE_BROWSER:
      requested_command = CommandsInfo::GetBrowserActionCommand(extension);
      break;
    case ActionInfo::TYPE_PAGE:
      requested_command = CommandsInfo::GetPageActionCommand(extension);
      break;
    case ActionInfo::TYPE_ACTION:
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
void
BrowserContextKeyedAPIFactory<CommandService>::DeclareFactoryDependencies() {
  DependsOn(ExtensionCommandsGlobalRegistry::GetFactoryInstance());
}

}  // namespace extensions
