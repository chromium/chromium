// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/error_console/error_console_factory.h"

#include "chrome/browser/extensions/error_console/error_console.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/browser/extension_registry_factory.h"
#include "extensions/browser/extensions_browser_client.h"

using content::BrowserContext;

namespace extensions {

// static
ErrorConsole* ErrorConsoleFactory::GetForBrowserContext(
    BrowserContext* context) {
  return static_cast<ErrorConsole*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
ErrorConsoleFactory* ErrorConsoleFactory::GetInstance() {
  return base::Singleton<ErrorConsoleFactory>::get();
}

ErrorConsoleFactory::ErrorConsoleFactory()
    : ProfileKeyedServiceFactory(
          "ErrorConsole",
          ProfileSelections::BuildRedirectedInIncognito()) {
  DependsOn(ExtensionRegistryFactory::GetInstance());
}

ErrorConsoleFactory::~ErrorConsoleFactory() {
}

KeyedService* ErrorConsoleFactory::BuildServiceInstanceFor(
    BrowserContext* context) const {
  return new ErrorConsole(Profile::FromBrowserContext(context));
}

}  // namespace extensions
