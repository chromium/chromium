// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_ACTION_MANAGER_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_ACTION_MANAGER_H_

#include <map>
#include <memory>
#include <string>

#include "base/scoped_observer.h"
#include "chrome/common/extensions/api/extension_action/action_info.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"

class ExtensionAction;
class Profile;

namespace extensions {

class Extension;

// Owns the ExtensionActions associated with each extension.  These actions live
// while an extension is loaded and are destroyed on unload.
class ExtensionActionManager : public KeyedService,
                               public ExtensionRegistryObserver {
 public:
  explicit ExtensionActionManager(Profile* profile);
  ~ExtensionActionManager() override;

  // Returns this profile's ExtensionActionManager.  One instance is
  // shared between a profile and its incognito version.
  static ExtensionActionManager* Get(content::BrowserContext* browser_context);

  // Returns the action associated with the extension (specified through the
  // "action", "browser_action", or "page_action" keys), or null if none exists.
  // Since an extension can only declare one of these, this is safe to use
  // anywhere callers simply need to get at the action and don't care about
  // the manifest key.
  ExtensionAction* GetExtensionAction(const Extension& extension) const;

 private:
  // Implement ExtensionRegistryObserver.
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const Extension* extension,
                           UnloadedExtensionReason reason) override;

  Profile* profile_;

  // Listen to extension unloaded notifications.
  ScopedObserver<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observer_{this};

  // Keyed by Extension ID.  These maps are populated lazily when their
  // ExtensionAction is first requested, and the entries are removed when the
  // extension is unloaded.  Not every extension has an action.
  using ExtIdToActionMap =
      std::map<std::string, std::unique_ptr<ExtensionAction>>;
  mutable ExtIdToActionMap actions_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_ACTION_MANAGER_H_
