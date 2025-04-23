// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/shared_module_service_factory.h"

#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "chrome/browser/extensions/shared_module_service.h"
#include "chrome/browser/extensions/updater/extension_updater_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "extensions/browser/delayed_install_manager_factory.h"
#include "extensions/browser/extension_registrar_factory.h"
#include "extensions/browser/extension_registry_factory.h"
#include "extensions/browser/pending_extension_manager_factory.h"

using content::BrowserContext;

namespace extensions {

// static
SharedModuleService* SharedModuleServiceFactory::GetForBrowserContext(
    BrowserContext* context) {
  return static_cast<SharedModuleService*>(
      GetInstance()->GetServiceForBrowserContext(context, /*create=*/true));
}

// static
SharedModuleServiceFactory* SharedModuleServiceFactory::GetInstance() {
  static base::NoDestructor<SharedModuleServiceFactory> instance;
  return instance.get();
}

SharedModuleServiceFactory::SharedModuleServiceFactory()
    : ProfileKeyedServiceFactory(
          "SharedModuleService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/40257657): Audit whether these should be
              // redirected or should have their own instance.
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kRedirectedToOriginal)
              .Build()) {
  DependsOn(DelayedInstallManagerFactory::GetInstance());
  DependsOn(ExtensionRegistrarFactory::GetInstance());
  DependsOn(ExtensionRegistryFactory::GetInstance());
  DependsOn(ExtensionUpdaterFactory::GetInstance());
  DependsOn(PendingExtensionManagerFactory::GetInstance());
}

SharedModuleServiceFactory::~SharedModuleServiceFactory() = default;

bool SharedModuleServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  // Create the service immediately so it can observe the `ExtensionRegistry`
  // for installs.
  return true;
}

std::unique_ptr<KeyedService>
SharedModuleServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  // Use `new` to access private constructor.
  return base::WrapUnique(new SharedModuleService(context));
}

}  // namespace extensions
