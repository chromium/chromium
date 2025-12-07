// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/corrupted_extension_reinstaller_factory.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/extensions/corrupted_extension_reinstaller.h"
#include "chrome/browser/extensions/external_provider_manager_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "extensions/buildflags/buildflags.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

using content::BrowserContext;

namespace extensions {

// static
CorruptedExtensionReinstaller*
CorruptedExtensionReinstallerFactory::GetForBrowserContext(
    BrowserContext* context) {
  return static_cast<CorruptedExtensionReinstaller*>(
      GetInstance()->GetServiceForBrowserContext(context, /*create=*/true));
}

// static
CorruptedExtensionReinstallerFactory*
CorruptedExtensionReinstallerFactory::GetInstance() {
  static base::NoDestructor<CorruptedExtensionReinstallerFactory> instance;
  return instance.get();
}

CorruptedExtensionReinstallerFactory::CorruptedExtensionReinstallerFactory()
    : ProfileKeyedServiceFactory(
          "CorruptedExtensionReinstaller",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/40257657): Audit whether these should be
              // redirected or should have their own instance.
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kRedirectedToOriginal)
              .Build()) {
  DependsOn(ExternalProviderManagerFactory::GetInstance());
}

CorruptedExtensionReinstallerFactory::~CorruptedExtensionReinstallerFactory() =
    default;

std::unique_ptr<KeyedService>
CorruptedExtensionReinstallerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  // Use `new` because the constructor is private.
  return base::WrapUnique(new CorruptedExtensionReinstaller(context));
}

}  // namespace extensions
