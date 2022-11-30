// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_BROWSER_WINDOW_HELPER_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_BROWSER_WINDOW_HELPER_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"

class Browser;

namespace extensions {

// A helper object for extensions-related management for Browser* objects.
class ExtensionBrowserWindowHelper : public ExtensionRegistryObserver {
 public:
  // Note: |browser| must outlive this object.
  explicit ExtensionBrowserWindowHelper(Browser* browser);

  ExtensionBrowserWindowHelper(const ExtensionBrowserWindowHelper&) = delete;
  ExtensionBrowserWindowHelper& operator=(const ExtensionBrowserWindowHelper&) =
      delete;

  ~ExtensionBrowserWindowHelper() override;

 private:
  // ExtensionRegistryObserver:
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const Extension* extension) override;
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const Extension* extension,
                           UnloadedExtensionReason reason) override;

  // Closes any tabs owned by the extension and unmutes others if necessary.
  void CleanUpTabsOnUnload(const Extension* extension);

  // The associated browser. Must outlive this object.
  const raw_ptr<Browser> browser_ = nullptr;

  base::ScopedObservation<ExtensionRegistry, ExtensionRegistryObserver>
      registry_observation_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_BROWSER_WINDOW_HELPER_H_
