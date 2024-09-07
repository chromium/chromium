// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_commands_global_registry.h"

#include "base/lazy_instance.h"
#include "chrome/browser/extensions/commands/command_service.h"
#include "chrome/browser/extensions/global_shortcut_listener.h"
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

    // Resume GlobalShortcutListener before we clean up if the shortcut handling
    // is currently suspended.
    if (global_shortcut_listener->IsShortcutHandlingSuspended())
      global_shortcut_listener->SetShortcutHandlingSuspended(false);

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
  if (ShouldIgnoreCommand(command_name))
    return;

  extensions::CommandService* command_service =
      extensions::CommandService::Get(browser_context_);
  // Add all the active global keybindings, if any.
  extensions::CommandMap commands;
  if (!command_service->GetNamedCommands(
          extension->id(),
          extensions::CommandService::ACTIVE,
          extensions::CommandService::GLOBAL,
          &commands))
    return;

  extensions::CommandMap::const_iterator iter = commands.begin();
  for (; iter != commands.end(); ++iter) {
    if (!command_name.empty() && (iter->second.command_name() != command_name))
      continue;
    const ui::Accelerator& accelerator = iter->second.accelerator();

    if (!IsAcceleratorRegistered(accelerator)) {
      if (!GlobalShortcutListener::GetInstance()->RegisterAccelerator(
              accelerator, this))
        continue;
    }

    AddEventTarget(accelerator, extension->id(), iter->second.command_name());
  }
}

void ExtensionCommandsGlobalRegistry::RemoveExtensionKeybindingImpl(
    const ui::Accelerator& accelerator,
    const std::string& command_name) {
  GlobalShortcutListener::GetInstance()->UnregisterAccelerator(
      accelerator, this);
}

void ExtensionCommandsGlobalRegistry::OnShortcutHandlingSuspended(
    bool suspended) {
  GlobalShortcutListener::GetInstance()->SetShortcutHandlingSuspended(
      suspended);
  if (registry_for_active_window())
    registry_for_active_window()->SetShortcutHandlingSuspended(suspended);
}

void ExtensionCommandsGlobalRegistry::OnKeyPressed(
    const ui::Accelerator& accelerator) {
  ExtensionKeybindingRegistry::NotifyEventTargets(accelerator);
}

}  // namespace extensions
