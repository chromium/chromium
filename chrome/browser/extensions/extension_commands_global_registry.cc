// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_commands_global_registry.h"

#include "base/lazy_instance.h"
#include "base/uuid.h"
#include "chrome/browser/extensions/commands/command_service.h"
#include "chrome/browser/extensions/global_shortcut_listener.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/pref_names.h"
#include "extensions/common/extension.h"

namespace extensions {

ExtensionCommandsGlobalRegistry::ExtensionCommandsGlobalRegistry(
    content::BrowserContext* context)
    : ExtensionKeybindingRegistry(context,
                                  ExtensionKeybindingRegistry::ALL_EXTENSIONS,
                                  nullptr),
      browser_context_(context),
      registry_for_active_window_(nullptr) {
  Init();
}

ExtensionCommandsGlobalRegistry::~ExtensionCommandsGlobalRegistry() {
  if (!IsEventTargetsEmpty()) {
    GlobalShortcutListener* global_shortcut_listener =
        GlobalShortcutListener::GetInstance();
    if (!global_shortcut_listener) {
      return;
    }

    // Resume GlobalShortcutListener before we clean up if the shortcut handling
    // is currently suspended.
    if (global_shortcut_listener->IsShortcutHandlingSuspended()) {
      global_shortcut_listener->SetShortcutHandlingSuspended(false);
    }

    global_shortcut_listener->UnregisterAccelerators(this);
  }
}

static base::LazyInstance<BrowserContextKeyedAPIFactory<
    ExtensionCommandsGlobalRegistry>>::DestructorAtExit
    g_extension_commands_global_registry_factory = LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<ExtensionCommandsGlobalRegistry>*
ExtensionCommandsGlobalRegistry::GetFactoryInstance() {
  return g_extension_commands_global_registry_factory.Pointer();
}

// static
ExtensionCommandsGlobalRegistry* ExtensionCommandsGlobalRegistry::Get(
    content::BrowserContext* context) {
  return BrowserContextKeyedAPIFactory<ExtensionCommandsGlobalRegistry>::Get(
      context);
}

bool ExtensionCommandsGlobalRegistry::IsRegistered(
    const ui::Accelerator& accelerator) {
  return (registry_for_active_window() &&
          registry_for_active_window()->IsAcceleratorRegistered(accelerator)) ||
         IsAcceleratorRegistered(accelerator);
}

void ExtensionCommandsGlobalRegistry::AddExtensionKeybindings(
    const extensions::Extension* extension,
    const std::string& command_name) {
  // This object only handles named commands, not browser/page actions.
  if (ShouldIgnoreCommand(command_name)) {
    return;
  }

  auto* instance = GlobalShortcutListener::GetInstance();
  if (!instance) {
    return;
  }
  extensions::CommandService* command_service =
      extensions::CommandService::Get(browser_context_);
  extensions::CommandMap commands;
  if (instance->IsRegistrationHandledExternally()) {
    if (!command_service->GetNamedCommands(
            extension->id(), extensions::CommandService::ALL,
            extensions::CommandService::ANY_SCOPE, &commands)) {
      return;
    }
    PrefService* prefs =
        ExtensionsBrowserClient::Get()->GetPrefServiceForContext(
            browser_context_);
    std::string profile_id = prefs->GetString(pref_names::kGlobalShortcutsUuid);
    if (profile_id.empty()) {
      auto uuid = base::Uuid::GenerateRandomV4();
      profile_id = uuid.AsLowercaseString();
      prefs->SetString(pref_names::kGlobalShortcutsUuid, profile_id);
    }
    instance->OnCommandsChanged(extension->id(), profile_id, commands, this);
  }

  // Add all the active global keybindings, if any.
  if (!command_service->GetNamedCommands(
          extension->id(), extensions::CommandService::ACTIVE,
          extensions::CommandService::GLOBAL, &commands)) {
    return;
  }

  for (auto& command : commands) {
    if (!command_name.empty() &&
        (command.second.command_name() != command_name)) {
      continue;
    }
    const ui::Accelerator& accelerator = command.second.accelerator();

    if (!IsAcceleratorRegistered(accelerator)) {
      if (!instance->RegisterAccelerator(accelerator, this)) {
        continue;
      }
    }

    AddEventTarget(accelerator, extension->id(), command.second.command_name());
  }
}

void ExtensionCommandsGlobalRegistry::RemoveExtensionKeybindingImpl(
    const ui::Accelerator& accelerator,
    const std::string& command_name) {
  auto* instance = GlobalShortcutListener::GetInstance();
  if (!instance) {
    return;
  }
  instance->UnregisterAccelerator(accelerator, this);
}

void ExtensionCommandsGlobalRegistry::OnShortcutHandlingSuspended(
    bool suspended) {
  auto* instance = GlobalShortcutListener::GetInstance();
  if (!instance) {
    return;
  }
  instance->SetShortcutHandlingSuspended(suspended);
  if (registry_for_active_window()) {
    registry_for_active_window()->SetShortcutHandlingSuspended(suspended);
  }
}

void ExtensionCommandsGlobalRegistry::OnKeyPressed(
    const ui::Accelerator& accelerator) {
  ExtensionKeybindingRegistry::NotifyEventTargets(accelerator);
}

void ExtensionCommandsGlobalRegistry::ExecuteCommand(
    const ExtensionId& extension_id,
    const std::string& command_id) {
  CommandExecuted(extension_id, command_id);
}

}  // namespace extensions
