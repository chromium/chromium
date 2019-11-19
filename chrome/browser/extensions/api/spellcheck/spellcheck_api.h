// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SPELLCHECK_SPELLCHECK_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_SPELLCHECK_SPELLCHECK_API_H_

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"

namespace extensions {

class SpellcheckAPI : public BrowserContextKeyedAPI,
                      public ExtensionRegistryObserver {
 public:
  explicit SpellcheckAPI(content::BrowserContext* context);
  ~SpellcheckAPI() override;

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<SpellcheckAPI>* GetFactoryInstance();

 private:
  friend class BrowserContextKeyedAPIFactory<SpellcheckAPI>;

  // ExtensionRegistryObserver implementation.
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const Extension* extension) override;
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const Extension* extension,
                           UnloadedExtensionReason reason) override;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() {
    return "SpellcheckAPI";
  }

  // Listen to extension load, unloaded notifications.
  ScopedObserver<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observer_{this};

  DISALLOW_COPY_AND_ASSIGN(SpellcheckAPI);
};

template <>
void BrowserContextKeyedAPIFactory<SpellcheckAPI>::DeclareFactoryDependencies();

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SPELLCHECK_SPELLCHECK_API_H_
