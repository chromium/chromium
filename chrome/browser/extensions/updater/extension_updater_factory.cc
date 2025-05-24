// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/updater/extension_updater_factory.h"

#include "chrome/browser/extensions/corrupted_extension_reinstaller_factory.h"
#include "chrome/browser/extensions/external_install_manager_factory.h"
#include "chrome/browser/extensions/forced_extensions/install_stage_tracker_factory.h"
#include "chrome/browser/extensions/updater/extension_updater.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "extensions/browser/delayed_install_manager_factory.h"
#include "extensions/browser/extension_prefs_factory.h"
#include "extensions/browser/extension_registrar_factory.h"
#include "extensions/browser/extension_registry_factory.h"
#include "extensions/browser/pending_extension_manager_factory.h"
#include "extensions/browser/updater/update_service_factory.h"

using content::BrowserContext;

namespace extensions {

// static
ExtensionUpdater* ExtensionUpdaterFactory::GetForBrowserContext(
    BrowserContext* context) {
  return static_cast<ExtensionUpdater*>(
      GetInstance()->GetServiceForBrowserContext(context, /*create=*/true));
}

// static
ExtensionUpdaterFactory* ExtensionUpdaterFactory::GetInstance() {
  static base::NoDestructor<ExtensionUpdaterFactory> instance;
  return instance.get();
}

ExtensionUpdaterFactory::ExtensionUpdaterFactory()
    : ProfileKeyedServiceFactory(
          "ExtensionUpdater",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/40257657): Audit whether these should be
              // redirected or should have their own instance.
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kRedirectedToOriginal)
              .Build()) {
  DependsOn(CorruptedExtensionReinstallerFactory::GetInstance());
  DependsOn(DelayedInstallManagerFactory::GetInstance());
  DependsOn(ExtensionPrefsFactory::GetInstance());
  DependsOn(ExtensionRegistrarFactory::GetInstance());
  DependsOn(ExtensionRegistryFactory::GetInstance());
  DependsOn(ExternalInstallManagerFactory::GetInstance());
  DependsOn(InstallStageTrackerFactory::GetInstance());
  DependsOn(PendingExtensionManagerFactory::GetInstance());
  DependsOn(UpdateServiceFactory::GetInstance());
}

ExtensionUpdaterFactory::~ExtensionUpdaterFactory() = default;

std::unique_ptr<KeyedService>
ExtensionUpdaterFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<ExtensionUpdater>(profile);
}

}  // namespace extensions
