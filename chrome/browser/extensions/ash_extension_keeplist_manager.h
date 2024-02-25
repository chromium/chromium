// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_ASH_EXTENSION_KEEPLIST_MANAGER_H_
#define CHROME_BROWSER_EXTENSIONS_ASH_EXTENSION_KEEPLIST_MANAGER_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/common/extension_id.h"

class Profile;

namespace extensions {
class ExtensionPrefs;
class ExtensionRegistry;
class ExtensionService;

// This class manages the 1st party Ash extension keeplist. When Lacros becomes
// the only browser, all extensions should be installed in Lacros instead of
// Ash. However, there is a small exception set of 1st party extensions and
// platform apps we will keep running in Ash, since they are either needed to
// support some Chrome OS features such as accessibility, or are in the process
// of deprecation, or not completely Lacros compatible yet. This class will
// manage to disable all the extensions and platform apps in Ash if they are
// not in the keep list.
class AshExtensionKeeplistManager : private ExtensionRegistryObserver {
 public:
  AshExtensionKeeplistManager(Profile* profile,
                              ExtensionPrefs* extension_prefs,
                              ExtensionService* extension_service);
  AshExtensionKeeplistManager(const AshExtensionKeeplistManager&) = delete;
  AshExtensionKeeplistManager& operator=(const AshExtensionKeeplistManager&) =
      delete;
  ~AshExtensionKeeplistManager() override;

  void Init();

 private:
  // Returns true if |extension| should be disabled.
  bool ShouldDisable(const Extension* extension) const;

  // Disables the extension with 'DISABLE_NOT_ASH_KEEPLISTED'.
  void Disable(const ExtensionId& extension_id);

  // Blocks all extensions not on the keeplist by disabling them with
  // 'DISABLE_NOT_ASH_KEEPLISTED'.
  void ActivateKeeplistEnforcement();

  // Unblocks all extensions by removing 'DISABLE_NOT_ASH_KEEPLISTED' from
  // disable reasons. It will be called when Lacros is not primary browser or
  // features::kEnforceAshExtensionKeeplist is turned off.
  void DeactivateKeeplistEnforcement();

  // ExtensionRegistryObserver:
  void OnExtensionReady(content::BrowserContext* browser_context,
                        const Extension* extension) override;

  // The |extension_prefs_|, |extension_service_| and |registry_| are passed
  // from ctor and owned by the caller, and they are guaranteed to outlive this
  // object.
  raw_ptr<ExtensionPrefs> const extension_prefs_ = nullptr;      // not owned
  raw_ptr<ExtensionService> const extension_service_ = nullptr;  // not owned
  raw_ptr<ExtensionRegistry> const registry_ = nullptr;          // not owned

  bool should_enforce_keeplist_ = false;

  base::ScopedObservation<ExtensionRegistry, ExtensionRegistryObserver>
      registry_observation_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_ASH_EXTENSION_KEEPLIST_MANAGER_H_
