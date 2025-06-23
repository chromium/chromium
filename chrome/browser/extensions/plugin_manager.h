// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_PLUGIN_MANAGER_H_
#define CHROME_BROWSER_EXTENSIONS_PLUGIN_MANAGER_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"

class Profile;

namespace content {
class BrowserContext;
}

namespace extensions {

class PluginManager : public BrowserContextKeyedAPI,
                      public ExtensionRegistryObserver {
 public:
  explicit PluginManager(content::BrowserContext* context);

  PluginManager(const PluginManager&) = delete;
  PluginManager& operator=(const PluginManager&) = delete;

  ~PluginManager() override;

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<PluginManager>* GetFactoryInstance();

 private:
  friend class BrowserContextKeyedAPIFactory<PluginManager>;

  // ExtensionRegistryObserver implementation.
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const Extension* extension) override;
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const Extension* extension,
                           UnloadedExtensionReason reason) override;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "PluginManager"; }
  static const bool kServiceIsNULLWhileTesting = true;

  raw_ptr<Profile> profile_;

  // Listen to extension load, unloaded notifications.
  base::ScopedObservation<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observation_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_PLUGIN_MANAGER_H_
