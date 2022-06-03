// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/blocklist_factory.h"
#include "chrome/browser/extensions/blocklist.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_prefs_factory.h"
#include "extensions/browser/extensions_browser_client.h"

using content::BrowserContext;

namespace extensions {

// static
Blocklist* BlocklistFactory::GetForBrowserContext(BrowserContext* context) {
  return static_cast<Blocklist*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
BlocklistFactory* BlocklistFactory::GetInstance() {
  return base::Singleton<BlocklistFactory>::get();
}

BlocklistFactory::BlocklistFactory()
    : BrowserContextKeyedServiceFactory(
          "Blocklist",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(extensions::ExtensionPrefsFactory::GetInstance());
}

BlocklistFactory::~BlocklistFactory() {}

KeyedService* BlocklistFactory::BuildServiceInstanceFor(
    BrowserContext* context) const {
  return new Blocklist(ExtensionPrefs::Get(context));
}

BrowserContext* BlocklistFactory::GetBrowserContextToUse(
    BrowserContext* context) const {
  // Redirected in incognito.
  return ExtensionsBrowserClient::Get()->GetOriginalContext(context);
}

}  // namespace extensions
