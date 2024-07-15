// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/chrome_app_icon_service_factory.h"

#include "chrome/browser/extensions/chrome_app_icon_service.h"
#include "extensions/browser/extension_registry_factory.h"

namespace extensions {

// static
ChromeAppIconService* ChromeAppIconServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<ChromeAppIconService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
ChromeAppIconServiceFactory* ChromeAppIconServiceFactory::GetInstance() {
  static base::NoDestructor<ChromeAppIconServiceFactory> instance;
  return instance.get();
}

ChromeAppIconServiceFactory::ChromeAppIconServiceFactory()
    : ProfileKeyedServiceFactory(
          "ChromeAppIconService",
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

ChromeAppIconServiceFactory::~ChromeAppIconServiceFactory() = default;

std::unique_ptr<KeyedService>
ChromeAppIconServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<ChromeAppIconService>(context);
}

}  // namespace extensions
