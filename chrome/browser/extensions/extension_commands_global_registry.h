// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_COMMANDS_GLOBAL_REGISTRY_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_COMMANDS_GLOBAL_REGISTRY_H_

#include <map>
#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/extensions/extension_keybinding_registry.h"
#include "chrome/browser/extensions/global_shortcut_listener.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "ui/base/accelerators/accelerator.h"

namespace content {
class BrowserContext;
}

namespace extensions {
class Extension;

// ExtensionCommandsGlobalRegistry is a class that handles the cross-platform
// implementation of the global shortcut registration for the Extension Commands
// API).
// Note: It handles regular extension commands (not browserAction and pageAction
// popups, which are not bindable to global shortcuts). This class registers the
// accelerators on behalf of the extensions and routes the commands to them via
// the BrowserEventRouter.
class ExtensionCommandsGlobalRegistry
    : public BrowserContextKeyedAPI,
      public ExtensionKeybindingRegistry,
      public GlobalShortcutListener::Observer {
 public:
  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<ExtensionCommandsGlobalRegistry>*
      GetFactoryInstance();

  // Convenience method to get the ExtensionCommandsGlobalRegistry for a
  // profile.
  static ExtensionCommandsGlobalRegistry* Get(content::BrowserContext* context);

  explicit ExtensionCommandsGlobalRegistry(content::BrowserContext* context);

  ExtensionCommandsGlobalRegistry(const ExtensionCommandsGlobalRegistry&) =
      delete;
  ExtensionCommandsGlobalRegistry& operator=(
      const ExtensionCommandsGlobalRegistry&) = delete;

  ~ExtensionCommandsGlobalRegistry() override;

  // Returns which non-global command registry is active (belonging to the
  // currently active window).
  ExtensionKeybindingRegistry* registry_for_active_window() {
    return registry_for_active_window_;
  }

  void set_registry_for_active_window(ExtensionKeybindingRegistry* registry) {
    registry_for_active_window_ = registry;
  }

  // Returns whether |accelerator| is registered on the registry for the active
  // window or on the global registry.
  bool IsRegistered(const ui::Accelerator& accelerator);

 private:
  friend class BrowserContextKeyedAPIFactory<ExtensionCommandsGlobalRegistry>;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() {
    return "ExtensionCommandsGlobalRegistry";
  }
  static const bool kServiceRedirectedInIncognito = true;

  // Overridden from ExtensionKeybindingRegistry:
  void AddExtensionKeybindings(const Extension* extension,
                               const std::string& command_name) override;
  void RemoveExtensionKeybindingImpl(const ui::Accelerator& accelerator,
                                     const std::string& command_name) override;
  void OnShortcutHandlingSuspended(bool suspended) override;

  // Called by the GlobalShortcutListener object when a shortcut this class has
  // registered for has been pressed.
  void OnKeyPressed(const ui::Accelerator& accelerator) override;

  // Weak pointer to our browser context. Not owned by us.
  raw_ptr<content::BrowserContext> browser_context_;

  // The global commands registry not only keeps track of global commands
  // registered, but also of which non-global command registry is active
  // (belonging to the currently active window). Only valid for TOOLKIT_VIEWS
  // and
  // NULL otherwise.
  raw_ptr<ExtensionKeybindingRegistry> registry_for_active_window_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_COMMANDS_GLOBAL_REGISTRY_H_
