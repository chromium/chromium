// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_PLUGIN_MANAGER_H_
#define CHROME_BROWSER_EXTENSIONS_PLUGIN_MANAGER_H_

#include <set>
#include <string>

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "components/nacl/common/buildflags.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/common/manifest_handlers/nacl_modules_handler.h"

class GURL;
class Profile;

namespace content {
class BrowserContext;
}

namespace extensions {

class PluginManager : public BrowserContextKeyedAPI,
                      public ExtensionRegistryObserver {
 public:
  explicit PluginManager(content::BrowserContext* context);
  ~PluginManager() override;

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<PluginManager>* GetFactoryInstance();

 private:
  friend class BrowserContextKeyedAPIFactory<PluginManager>;

#if BUILDFLAG(ENABLE_NACL)

  // We implement some Pepper plugins using NaCl to take advantage of NaCl's
  // strong sandbox. Typically, these NaCl modules are stored in extensions
  // and registered here. Not all NaCl modules need to register for a MIME
  // type, just the ones that are responsible for rendering a particular MIME
  // type, like application/pdf. Note: We only register NaCl modules in the
  // browser process.
  void RegisterNaClModule(const NaClModuleInfo& info);
  void UnregisterNaClModule(const NaClModuleInfo& info);

  // Call UpdatePluginListWithNaClModules() after registering or unregistering
  // a NaCl module to see those changes reflected in the PluginList.
  void UpdatePluginListWithNaClModules();

  extensions::NaClModuleInfo::List::iterator FindNaClModule(const GURL& url);

#endif  // BUILDFLAG(ENABLE_NACL)

  // ExtensionRegistryObserver implementation.
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const Extension* extension) override;
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const Extension* extension,
                           UnloadedExtensionReason reason) override;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "PluginManager"; }
  static const bool kServiceIsNULLWhileTesting = true;

  extensions::NaClModuleInfo::List nacl_module_list_;

  Profile* profile_;

  // Listen to extension load, unloaded notifications.
  ScopedObserver<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observer_{this};

  DISALLOW_COPY_AND_ASSIGN(PluginManager);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_PLUGIN_MANAGER_H_
