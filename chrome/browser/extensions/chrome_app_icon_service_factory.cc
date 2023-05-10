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
  return base::Singleton<ChromeAppIconServiceFactory>::get();
}

ChromeAppIconServiceFactory::ChromeAppIconServiceFactory()
    : ProfileKeyedServiceFactory(
          "ChromeAppIconService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              .Build()) {
  DependsOn(ExtensionRegistryFactory::GetInstance());
}

ChromeAppIconServiceFactory::~ChromeAppIconServiceFactory() = default;

KeyedService* ChromeAppIconServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new ChromeAppIconService(context);
}

}  // namespace extensions
