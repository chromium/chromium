// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_error_controller_factory.h"

#include "base/check.h"
#include "chrome/browser/extensions/extension_error_controller.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "extensions/browser/extension_prefs_factory.h"
#include "extensions/browser/extension_registry_factory.h"
#include "extensions/browser/extension_system_provider.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/pending_extension_manager_factory.h"
#include "extensions/buildflags/buildflags.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

using content::BrowserContext;

namespace extensions {

// static
ExtensionErrorController* ExtensionErrorControllerFactory::GetForBrowserContext(
    BrowserContext* context) {
  return static_cast<ExtensionErrorController*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
ExtensionErrorControllerFactory*
ExtensionErrorControllerFactory::GetInstance() {
  static base::NoDestructor<ExtensionErrorControllerFactory> instance;
  return instance.get();
}

ExtensionErrorControllerFactory::ExtensionErrorControllerFactory()
    : ProfileKeyedServiceFactory(
          "ExtensionErrorController",
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
  DependsOn(PendingExtensionManagerFactory::GetInstance());
  DependsOn(ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
}

ExtensionErrorControllerFactory::~ExtensionErrorControllerFactory() = default;

std::unique_ptr<KeyedService>
ExtensionErrorControllerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<ExtensionErrorController>(context);
}

}  // namespace extensions
