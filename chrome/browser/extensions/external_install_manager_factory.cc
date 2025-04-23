// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/external_install_manager_factory.h"

#include "base/check.h"
#include "chrome/browser/extensions/external_install_manager.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "extensions/browser/extension_prefs_factory.h"
#include "extensions/browser/extension_registry_factory.h"

using content::BrowserContext;

namespace extensions {

// static
ExternalInstallManager* ExternalInstallManagerFactory::GetForBrowserContext(
    BrowserContext* context) {
  return static_cast<ExternalInstallManager*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
ExternalInstallManagerFactory* ExternalInstallManagerFactory::GetInstance() {
  static base::NoDestructor<ExternalInstallManagerFactory> instance;
  return instance.get();
}

ExternalInstallManagerFactory::ExternalInstallManagerFactory()
    : ProfileKeyedServiceFactory(
          "ExternalInstallManager",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/40257657): Audit whether these should be
              // redirected or should have their own instance.
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kRedirectedToOriginal)
              .Build()) {
  DependsOn(ExtensionRegistryFactory::GetInstance());
  DependsOn(ExtensionPrefsFactory::GetInstance());
}

ExternalInstallManagerFactory::~ExternalInstallManagerFactory() = default;

std::unique_ptr<KeyedService>
ExternalInstallManagerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<ExternalInstallManager>(context);
}

}  // namespace extensions
