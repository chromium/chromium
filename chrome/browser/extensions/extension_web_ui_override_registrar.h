// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_WEB_UI_OVERRIDE_REGISTRAR_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_WEB_UI_OVERRIDE_REGISTRAR_H_

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/buildflags/buildflags.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace content {
class BrowserContext;
}

namespace extensions {

class ExtensionWebUIOverrideRegistrar : public BrowserContextKeyedAPI,
                                        public ExtensionRegistryObserver {
 public:
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;

    // Called when an extension with a URL override is added and enabled.
    virtual void OnExtensionOverrideAdded(const Extension& extension) = 0;

    // Called when an extension with a URL override is removed or disabled.
    virtual void OnExtensionOverrideRemoved(const Extension& extension) = 0;
  };

  explicit ExtensionWebUIOverrideRegistrar(content::BrowserContext* context);

  ExtensionWebUIOverrideRegistrar(const ExtensionWebUIOverrideRegistrar&) =
      delete;
  ExtensionWebUIOverrideRegistrar& operator=(
      const ExtensionWebUIOverrideRegistrar&) = delete;

  ~ExtensionWebUIOverrideRegistrar() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<ExtensionWebUIOverrideRegistrar>*
      GetFactoryInstance();

 private:
  friend class BrowserContextKeyedAPIFactory<ExtensionWebUIOverrideRegistrar>;

  // ExtensionRegistryObserver implementation.
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const Extension* extension) override;
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const Extension* extension,
                           UnloadedExtensionReason reason) override;
  void OnExtensionUninstalled(content::BrowserContext* browser_context,
                              const Extension* extension,
                              UninstallReason reason) override;

  void OnExtensionSystemReady(content::BrowserContext* context);

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() {
    return "ExtensionWebUIOverrideRegistrar";
  }
  static const bool kServiceIsNULLWhileTesting = true;

  // Listen to extension load, unloaded notifications.
  base::ScopedObservation<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observation_{this};

  base::ObserverList<Observer> observer_list_;

  base::WeakPtrFactory<ExtensionWebUIOverrideRegistrar> weak_factory_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_WEB_UI_OVERRIDE_REGISTRAR_H_
