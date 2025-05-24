// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_allowlist_factory.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/extensions/extension_allowlist.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/safe_browsing_metrics_collector_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "extensions/browser/extension_prefs_factory.h"
#include "extensions/browser/extension_registrar_factory.h"
#include "extensions/browser/extension_registry_factory.h"

using content::BrowserContext;

namespace extensions {

// static
ExtensionAllowlist* ExtensionAllowlistFactory::GetForBrowserContext(
    BrowserContext* context) {
  return static_cast<ExtensionAllowlist*>(
      GetInstance()->GetServiceForBrowserContext(context, /*create=*/true));
}

// static
ExtensionAllowlistFactory* ExtensionAllowlistFactory::GetInstance() {
  static base::NoDestructor<ExtensionAllowlistFactory> instance;
  return instance.get();
}

ExtensionAllowlistFactory::ExtensionAllowlistFactory()
    : ProfileKeyedServiceFactory(
          "ExtensionAllowlist",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/40257657): Audit whether these should be
              // redirected or should have their own instance.
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kRedirectedToOriginal)
              .Build()) {
  DependsOn(ExtensionPrefsFactory::GetInstance());
  DependsOn(ExtensionRegistrarFactory::GetInstance());
  DependsOn(ExtensionRegistryFactory::GetInstance());
  DependsOn(safe_browsing::SafeBrowsingMetricsCollectorFactory::GetInstance());
}

ExtensionAllowlistFactory::~ExtensionAllowlistFactory() = default;

std::unique_ptr<KeyedService>
ExtensionAllowlistFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  // Use `new` because the constructor is private.
  return base::WrapUnique(new ExtensionAllowlist(profile));
}

}  // namespace extensions
