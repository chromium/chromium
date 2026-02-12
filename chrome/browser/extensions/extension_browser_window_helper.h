// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_BROWSER_WINDOW_HELPER_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_BROWSER_WINDOW_HELPER_H_

#include "base/memory/raw_ref.h"
#include "base/scoped_observation.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/buildflags/buildflags.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

class BrowserWindowInterface;
class Profile;

namespace extensions {

// A helper object for extensions-related management for browser objects.
// It is owned by `BrowserWindowFeatures` or `AndroidBrowserWindow`.
class ExtensionBrowserWindowHelper : public ExtensionRegistryObserver {
 public:
  // Takes a BrowserWindowInterface instead of TabListInterface because the tab
  // list may not be constructed by the time this object is created.
  ExtensionBrowserWindowHelper(BrowserWindowInterface* browser,
                               Profile* profile);

  ExtensionBrowserWindowHelper(const ExtensionBrowserWindowHelper&) = delete;
  ExtensionBrowserWindowHelper& operator=(const ExtensionBrowserWindowHelper&) =
      delete;

  ~ExtensionBrowserWindowHelper() override;

 private:
  // ExtensionRegistryObserver:
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const Extension* extension,
                           UnloadedExtensionReason reason) override;

  // Closes any tabs owned by the extension and unmutes others if necessary.
  void CleanUpTabsOnUnload(const Extension* extension);

  // These pointers come from the associated Browser object and it will ensure
  // they outlive this object.
  const raw_ref<BrowserWindowInterface> browser_;

  base::ScopedObservation<ExtensionRegistry, ExtensionRegistryObserver>
      registry_observation_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_BROWSER_WINDOW_HELPER_H_
