// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/delayed_install_manager_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/extensions/delayed_install_manager.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "extensions/browser/extension_prefs_factory.h"
#include "extensions/browser/extension_registrar_factory.h"

using content::BrowserContext;

namespace extensions {

// static
DelayedInstallManager* DelayedInstallManagerFactory::GetForBrowserContext(
    BrowserContext* context) {
  return static_cast<DelayedInstallManager*>(
      GetInstance()->GetServiceForBrowserContext(context, /*create=*/true));
}

// static
DelayedInstallManagerFactory* DelayedInstallManagerFactory::GetInstance() {
  static base::NoDestructor<DelayedInstallManagerFactory> instance;
  return instance.get();
}

DelayedInstallManagerFactory::DelayedInstallManagerFactory()
    : ProfileKeyedServiceFactory(
          "DelayedInstallManager",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kRedirectedToOriginal)
              .Build()) {
  DependsOn(ExtensionPrefsFactory::GetInstance());
  DependsOn(ExtensionRegistrarFactory::GetInstance());
}

DelayedInstallManagerFactory::~DelayedInstallManagerFactory() = default;

std::unique_ptr<KeyedService>
DelayedInstallManagerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<DelayedInstallManager>(context);
}

}  // namespace extensions
