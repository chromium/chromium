// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_KEYBINDING_REGISTRY_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_KEYBINDING_REGISTRY_H_

#include <list>
#include <map>
#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/extensions/commands/command_service.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/common/extension_id.h"
#include "ui/base/accelerators/media_keys_listener.h"

namespace content {
class BrowserContext;
class WebContents;
}

namespace ui {
class Accelerator;
}

namespace extensions {

class Extension;

// The ExtensionKeybindingRegistry is a class that handles the cross-platform
// logic for keyboard accelerators. See platform-specific implementations for
// implementation details for each platform.
class ExtensionKeybindingRegistry : public CommandService::Observer,
                                    public ExtensionRegistryObserver,
                                    public ui::MediaKeysListener::Delegate {
 public:
  enum ExtensionFilter {
    ALL_EXTENSIONS,
    PLATFORM_APPS_ONLY
  };

  class Delegate {
   public:
    // Returns the currently active WebContents, or nullptr if there is none.
    virtual content::WebContents* GetWebContentsForExtension() = 0;
  };

  // If |extension_filter| is not ALL_EXTENSIONS, only keybindings by
  // by extensions that match the filter will be registered.
  ExtensionKeybindingRegistry(content::BrowserContext* context,
                              ExtensionFilter extension_filter,
                              Delegate* delegate);

  ExtensionKeybindingRegistry(const ExtensionKeybindingRegistry&) = delete;
  ExtensionKeybindingRegistry& operator=(const ExtensionKeybindingRegistry&) =
      delete;

  ~ExtensionKeybindingRegistry() override;

  // Enables/Disables general shortcut handling in Chrome.
  void SetShortcutHandlingSuspended(bool suspended);
  bool shortcut_handling_suspended() const {
    return shortcut_handling_suspended_;
  }

  // Check whether the specified |accelerator| has been registered.
  bool IsAcceleratorRegistered(const ui::Accelerator& accelerator) const;

 protected:
  // Add extension keybindings for the events defined by the |extension|.
  // |command_name| is optional, but if not blank then only the command
  // specified will be added.
  virtual void AddExtensionKeybindings(
      const Extension* extension,
      const std::string& command_name) = 0;
  // Remove extension bindings for |extension|. |command_name| is optional,
  // but if not blank then only the command specified will be removed.
  void RemoveExtensionKeybinding(
      const Extension* extension,
      const std::string& command_name);
  // Overridden by platform specific implementations to provide additional
  // unregistration (which varies between platforms).
  virtual void RemoveExtensionKeybindingImpl(
      const ui::Accelerator& accelerator,
      const std::string& command_name) = 0;

  // Called when shortcut handling is suspended or resumed.
  virtual void OnShortcutHandlingSuspended(bool suspended) {}

  // Make sure all extensions registered have keybindings added.
  void Init();

  // Whether to ignore this command. Only browserAction commands and pageAction
  // commands are currently ignored, since they are handled elsewhere.
  bool ShouldIgnoreCommand(const std::string& command) const;

  // Fire event targets which the specified |accelerator| is binding with.
  // Returns true if we can find the appropriate event targets.
  bool NotifyEventTargets(const ui::Accelerator& accelerator);

  // Notifies appropriate parties that a command has been executed.
  void CommandExecuted(const ExtensionId& extension_id,
                       const std::string& command);

  // Add event target (extension_id, command name) to the target list of
  // |accelerator|. Note that only media keys can have more than one event
  // target.
  void AddEventTarget(const ui::Accelerator& accelerator,
                      const ExtensionId& extension_id,
                      const std::string& command_name);

  // Get the first event target by the given |accelerator|. For a valid
  // accelerator it should have only one event target, except for media keys.
  // Returns true if we can find it, |extension_id| and |command_name| will be
  // set to the right target; otherwise, false is returned and |extension_id|,
  // |command_name| are unchanged.
  bool GetFirstTarget(const ui::Accelerator& accelerator,
                      ExtensionId* extension_id,
                      std::string* command_name) const;

  // Returns true if the |event_targets_| is empty; otherwise returns false.
  bool IsEventTargetsEmpty() const;

  // Returns the BrowserContext for this registry.
  content::BrowserContext* browser_context() const { return browser_context_; }

 private:
  // extensions::CommandService::Observer:
  void OnExtensionCommandAdded(const ExtensionId& extension_id,
                               const Command& command) override;
  void OnExtensionCommandRemoved(const ExtensionId& extension_id,
                                 const Command& command) override;
  void OnCommandServiceDestroying() override;

  // ExtensionRegistryObserver implementation.
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const Extension* extension) override;
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const Extension* extension,
                           UnloadedExtensionReason reason) override;

  // ui::MediaKeysListener::Delegate:
  void OnMediaKeysAccelerator(const ui::Accelerator& accelerator) override;

  // Returns true if the |extension| matches our extension filter.
  bool ExtensionMatchesFilter(const extensions::Extension* extension);

  // Execute commands for |accelerator|. If |extension_id| is empty, execute all
  // commands bound to |accelerator|, otherwise execute only commands bound by
  // the corresponding extension. Returns true if at least one command was
  // executed.
  bool ExecuteCommands(const ui::Accelerator& accelerator,
                       const ExtensionId& extension_id);

  // Returns true if any media keys are registered.
  bool IsListeningToAnyMediaKeys() const;

  raw_ptr<content::BrowserContext> browser_context_;

  // What extensions to register keybindings for.
  ExtensionFilter extension_filter_;

  // Weak pointer to our delegate. Not owned by us. Must outlive this class.
  raw_ptr<Delegate> delegate_;

  // Maps an accelerator to a list of string pairs (extension id, command name)
  // for commands that have been registered. This keeps track of the targets for
  // the keybinding event (which named command to call in which extension). On
  // GTK this map contains registration for pageAction and browserAction
  // commands, whereas on other platforms it does not. Note that normal
  // accelerator (which isn't media keys) has only one target, while the media
  // keys can have more than one.
  typedef std::list<std::pair<ExtensionId, std::string>> TargetList;
  typedef std::map<ui::Accelerator, TargetList> EventTargets;
  EventTargets event_targets_;

  // Listen to extension load, unloaded notifications.
  base::ScopedObservation<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observation_{this};

  base::ScopedObservation<CommandService, CommandService::Observer>
      command_service_observation_{this};

  // Keeps track of whether shortcut handling is currently suspended. Shortcuts
  // are suspended briefly while capturing which shortcut to assign to an
  // extension command in the Config UI. If handling isn't suspended while
  // capturing then trying to assign Ctrl+F to a command would instead result
  // in the Find box opening.
  bool shortcut_handling_suspended_;

  // Listen for Media keys events.
  std::unique_ptr<ui::MediaKeysListener> media_keys_listener_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_KEYBINDING_REGISTRY_H_
