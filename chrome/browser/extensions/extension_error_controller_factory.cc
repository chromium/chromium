// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_error_controller_factory.h"

#include "base/check.h"
#include "chrome/browser/extensions/extension_error_controller.h"
#include "chrome/browser/extensions/pending_extension_manager_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "extensions/browser/extension_prefs_factory.h"
#include "extensions/browser/extension_registry_factory.h"
#include "extensions/browser/extensions_browser_client.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/extensions/chrome_extension_system_factory.h"
#endif

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
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kRedirectedToOriginal)
              .Build()) {
  DependsOn(ExtensionRegistryFactory::GetInstance());
  DependsOn(ExtensionPrefsFactory::GetInstance());
  DependsOn(PendingExtensionManagerFactory::GetInstance());
  // TODO(crbug.com/394876083): `ExtensionSystem` is used by
  // `ExtensionErrorController` to access `management_policy()`. Since
  // `ManagementPolicy` is not supported on desktop android yet,
  // `ExtensionSystem` is not used on desktop android. Port the following code
  // when policy management is supported.
#if !BUILDFLAG(IS_ANDROID)
  DependsOn(ChromeExtensionSystemFactory::GetInstance());
#endif
}

ExtensionErrorControllerFactory::~ExtensionErrorControllerFactory() = default;

std::unique_ptr<KeyedService>
ExtensionErrorControllerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<ExtensionErrorController>(context);
}

}  // namespace extensions
