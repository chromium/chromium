// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_ACTION_STORAGE_MANAGER_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_ACTION_STORAGE_MANAGER_H_

#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "chrome/browser/extensions/api/extension_action/extension_action_api.h"
#include "chrome/browser/extensions/extension_action.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"

namespace content {
class BrowserContext;
}

namespace extensions {
class StateStore;

// This class manages reading and writing browser action values from storage.
class ExtensionActionStorageManager : public ExtensionActionAPI::Observer,
                                      public ExtensionRegistryObserver {
 public:
  explicit ExtensionActionStorageManager(content::BrowserContext* context);
  ~ExtensionActionStorageManager() override;

 private:
  // ExtensionActionAPI::Observer:
  void OnExtensionActionUpdated(
      ExtensionAction* extension_action,
      content::WebContents* web_contents,
      content::BrowserContext* browser_context) override;
  void OnExtensionActionAPIShuttingDown() override;

  // ExtensionRegistryObserver:
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const Extension* extension) override;

  // Reads/Writes the ExtensionAction's default values to/from storage.
  void WriteToStorage(ExtensionAction* extension_action);
  void ReadFromStorage(const std::string& extension_id,
                       std::unique_ptr<base::Value> value);

  // Returns the Extensions StateStore for the |browser_context_|.
  // May return NULL.
  StateStore* GetStateStore();

  content::BrowserContext* browser_context_;

  ScopedObserver<ExtensionActionAPI, ExtensionActionAPI::Observer>
      extension_action_observer_{this};

  ScopedObserver<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observer_{this};

  base::WeakPtrFactory<ExtensionActionStorageManager> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ExtensionActionStorageManager);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_ACTION_STORAGE_MANAGER_H_
