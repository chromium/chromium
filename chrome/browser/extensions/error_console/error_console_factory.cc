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
  static base::NoDestructor<ErrorConsoleFactory> instance;
  return instance.get();
}

ErrorConsoleFactory::ErrorConsoleFactory()
    : ProfileKeyedServiceFactory(
          "ErrorConsole",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kRedirectedToOriginal)
              .Build()) {
  DependsOn(ExtensionRegistryFactory::GetInstance());
}

ErrorConsoleFactory::~ErrorConsoleFactory() = default;

std::unique_ptr<KeyedService>
ErrorConsoleFactory::BuildServiceInstanceForBrowserContext(
    BrowserContext* context) const {
  return std::make_unique<ErrorConsole>(Profile::FromBrowserContext(context));
}

}  // namespace extensions
