// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/component_loader_factory.h"

#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "extensions/browser/extension_registrar_factory.h"
#include "extensions/browser/extension_system_provider.h"
#include "extensions/browser/extensions_browser_client.h"

using content::BrowserContext;

namespace extensions {

// static
ComponentLoader* ComponentLoaderFactory::GetForBrowserContext(
    BrowserContext* context) {
  return static_cast<ComponentLoader*>(
      GetInstance()->GetServiceForBrowserContext(context, /*create=*/true));
}

// static
ComponentLoaderFactory* ComponentLoaderFactory::GetInstance() {
  static base::NoDestructor<ComponentLoaderFactory> instance;
  return instance.get();
}

ComponentLoaderFactory::ComponentLoaderFactory()
    : ProfileKeyedServiceFactory(
          "ComponentLoader",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/40257657): Audit whether these should be
              // redirected or should have their own instance.
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kRedirectedToOriginal)
              .Build()) {
  DependsOn(ExtensionRegistrarFactory::GetInstance());
  DependsOn(ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
}

ComponentLoaderFactory::~ComponentLoaderFactory() = default;

std::unique_ptr<KeyedService>
ComponentLoaderFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  // Use `new` to access private constructor.
  return base::WrapUnique(new ComponentLoader(profile));
}

}  // namespace extensions
