// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/blocklist_factory.h"

#include "extensions/browser/blocklist.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_prefs_factory.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/buildflags/buildflags.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

using content::BrowserContext;

namespace extensions {

// static
Blocklist* BlocklistFactory::GetForBrowserContext(BrowserContext* context) {
  return static_cast<Blocklist*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
BlocklistFactory* BlocklistFactory::GetInstance() {
  static base::NoDestructor<BlocklistFactory> instance;
  return instance.get();
}

BlocklistFactory::BlocklistFactory()
    : ProfileKeyedServiceFactory(
          "Blocklist",
          // Redirected in incognito.
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/40257657): Audit whether these should be
              // redirected or should have their own instance.
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kRedirectedToOriginal)
              .Build()) {
  DependsOn(ExtensionPrefsFactory::GetInstance());
}

BlocklistFactory::~BlocklistFactory() = default;

std::unique_ptr<KeyedService>
BlocklistFactory::BuildServiceInstanceForBrowserContext(
    BrowserContext* context) const {
  return std::make_unique<Blocklist>(context);
}

}  // namespace extensions
