// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOCOMPLETE_SHORTCUTS_EXTENSIONS_MANAGER_H_
#define CHROME_BROWSER_AUTOCOMPLETE_SHORTCUTS_EXTENSIONS_MANAGER_H_

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "base/supports_user_data.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"

class Profile;

// This class manages the removal of shortcuts associated with an extension when
// that extension is unloaded.
class ShortcutsExtensionsManager
    : public base::SupportsUserData::Data,
      public extensions::ExtensionRegistryObserver {
 public:
  explicit ShortcutsExtensionsManager(Profile* profile);
  ~ShortcutsExtensionsManager() override;

  // extensions::ExtensionRegistryObserver:
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const extensions::Extension* extension,
                           extensions::UnloadedExtensionReason reason) override;
  void OnShutdown(extensions::ExtensionRegistry* registry) override;

 private:
  ScopedObserver<extensions::ExtensionRegistry,
                 extensions::ExtensionRegistryObserver>
      registry_observer_{this};
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(ShortcutsExtensionsManager);
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_SHORTCUTS_EXTENSIONS_MANAGER_H_
