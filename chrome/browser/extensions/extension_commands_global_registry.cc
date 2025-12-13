// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_commands_global_registry.h"

#include "base/lazy_instance.h"
#include "base/uuid.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/commands/command_service.h"
#include "chrome/browser/extensions/extension_keybinding_registry.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/pref_names.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension.h"
#include "ui/base/accelerators/command.h"
#include "ui/base/accelerators/global_accelerator_listener/global_accelerator_listener.h"

#if defined(USE_AURA) && !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/gfx/native_ui_types.h"
#endif

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

namespace {

gfx::AcceleratedWidget GetAcceleratedWidgetForContext(
    content::BrowserContext* context) {
#if defined(USE_AURA) && !BUILDFLAG(IS_ANDROID)
  auto* profile = Profile::FromBrowserContext(context);
  if (!profile) {
    return gfx::kNullAcceleratedWidget;
  }

  Browser* browser =
      chrome::FindLastActiveWithProfile(Profile::FromBrowserContext(context));
  if (!browser || !browser->window()) {
    return gfx::kNullAcceleratedWidget;
  }

  auto* native_window = browser->window()->GetNativeWindow();
  if (!native_window || !native_window->GetHost()) {
    return gfx::kNullAcceleratedWidget;
  }

  return native_window->GetHost()->GetAcceleratedWidget();
#else
  return gfx::kNullAcceleratedWidget;
#endif  // defined(USE_AURA) && !BUILDFLAG(IS_ANDROID)
}

}  // namespace

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
    ui::GlobalAcceleratorListener* global_shortcut_listener =
        ui::GlobalAcceleratorListener::GetInstance();
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

bool ExtensionCommandsGlobalRegistry::PopulateCommands(
    const Extension* extension,
    ui::CommandMap* commands) {
  auto* instance = ui::GlobalAcceleratorListener::GetInstance();
  if (!instance) {
    return false;
  }

  extensions::CommandService* command_service =
      extensions::CommandService::Get(browser_context_);
  if (instance->IsRegistrationHandledExternally()) {
    if (!command_service->GetNamedCommands(
            extension->id(), extensions::CommandService::ALL,
            extensions::CommandService::ANY_SCOPE, commands)) {
      return false;
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

    instance->OnCommandsChanged(
        extension->id(), profile_id, *commands,
        GetAcceleratedWidgetForContext(browser_context_), this);
  }

  // Add all the active global keybindings, if any.
  if (!command_service->GetNamedCommands(
          extension->id(), extensions::CommandService::ACTIVE,
          extensions::CommandService::GLOBAL, commands)) {
    return false;
  }
  return true;
}

bool ExtensionCommandsGlobalRegistry::RegisterAccelerator(
    const ui::Accelerator& accelerator,
    const ExtensionId& extension_id,
    const std::string& command_name) {
  auto* instance = ui::GlobalAcceleratorListener::GetInstance();
  if (!instance) {
    return false;
  }
  return instance->RegisterAccelerator(accelerator, this);
}

void ExtensionCommandsGlobalRegistry::UnregisterAccelerator(
    const ui::Accelerator& accelerator) {
  auto* instance = ui::GlobalAcceleratorListener::GetInstance();
  if (!instance) {
    return;
  }
  instance->UnregisterAccelerator(accelerator, this);
}

void ExtensionCommandsGlobalRegistry::OnShortcutHandlingSuspended(
    bool suspended) {
  auto* instance = ui::GlobalAcceleratorListener::GetInstance();
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
