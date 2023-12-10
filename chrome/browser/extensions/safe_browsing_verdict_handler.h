// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_SAFE_BROWSING_VERDICT_HANDLER_H_
#define CHROME_BROWSER_EXTENSIONS_SAFE_BROWSING_VERDICT_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/extensions/blocklist.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/common/extension_set.h"

namespace extensions {
class ExtensionPrefs;
class ExtensionService;

// Manages the Safe Browsing blocklist/greylist state in extension pref.
class SafeBrowsingVerdictHandler : public ExtensionRegistryObserver {
 public:
  SafeBrowsingVerdictHandler(ExtensionPrefs* extension_prefs,
                             ExtensionRegistry* registry,
                             ExtensionService* extension_service);
  SafeBrowsingVerdictHandler(const SafeBrowsingVerdictHandler&) = delete;
  SafeBrowsingVerdictHandler& operator=(const SafeBrowsingVerdictHandler&) =
      delete;
  ~SafeBrowsingVerdictHandler() override;

  // Initializes and load greylist from prefs.
  void Init();

  // Manages the blocklisted extensions. Enables/disables/loads/unloads
  // extensions based on the current `state_map`.
  void ManageBlocklist(const Blocklist::BlocklistStateMap& state_map);

 private:
  // Adds extensions in `blocklist` to `blocklist_` and maybe unload them.
  // Removes extensions that are neither in `blocklist`, nor in `unchanged` from
  // `blocklist_` and maybe reload them.
  void UpdateBlocklistedExtensions(const ExtensionIdSet& blocklist,
                                   const ExtensionIdSet& unchanged);

  // Adds extensions in `greylist` to `greylist_` and disables them. Removes
  // extensions that are neither in `greylist`, nor in `unchanged` from
  // `greylist_` and maybe re-enable them.
  void UpdateGreylistedExtensions(
      const ExtensionIdSet& greylist,
      const ExtensionIdSet& unchanged,
      const Blocklist::BlocklistStateMap& state_map);

  // ExtensionRegistryObserver overrides.
  void OnExtensionUninstalled(content::BrowserContext* browser_context,
                              const Extension* extension,
                              UninstallReason reason) override;

  base::ScopedObservation<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observation_{this};

  raw_ptr<ExtensionPrefs> extension_prefs_ = nullptr;
  raw_ptr<ExtensionRegistry> registry_ = nullptr;
  raw_ptr<ExtensionService> extension_service_ = nullptr;

  // Set of blocklisted extensions. These extensions are unloaded if they are
  // already installed in Chromium at the time when they are added to
  // the blocklist. This blocklist_ only contains extensions blocklisted by Safe
  // Browsing while ExtensionRegistry::blocklisted_extensions_ contains
  // extensions blocklisted by other sources such as Omaha attribute.
  ExtensionSet blocklist_;
  // Set of greylisted extensions. These extensions are disabled if they are
  // already installed in Chromium at the time when they are added to
  // the greylist. Unlike blocklisted extensions, greylisted ones are visible
  // to the user and if user re-enables such an extension, they remain enabled.
  //
  // These extensions should appear in registry_.
  ExtensionSet greylist_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_SAFE_BROWSING_VERDICT_HANDLER_H_
