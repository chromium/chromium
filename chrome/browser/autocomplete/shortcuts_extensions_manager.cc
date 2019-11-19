// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/shortcuts_extensions_manager.h"

#include "chrome/browser/autocomplete/shortcuts_backend_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/omnibox/browser/shortcuts_backend.h"
#include "extensions/common/extension.h"

ShortcutsExtensionsManager::ShortcutsExtensionsManager(Profile* profile)
    : profile_(profile) {
  DCHECK(profile_);
  registry_observer_.Add(extensions::ExtensionRegistry::Get(profile_));
}

ShortcutsExtensionsManager::~ShortcutsExtensionsManager() {}

void ShortcutsExtensionsManager::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UnloadedExtensionReason reason) {
  scoped_refptr<ShortcutsBackend> shortcuts_backend =
      ShortcutsBackendFactory::GetForProfileIfExists(profile_);
  if (!shortcuts_backend)
    return;

  // When an extension is unloaded, we want to remove any Shortcuts associated
  // with it.
  shortcuts_backend->DeleteShortcutsBeginningWithURL(extension->url());
}

void ShortcutsExtensionsManager::OnShutdown(
    extensions::ExtensionRegistry* registry) {
  registry_observer_.RemoveAll();
}
