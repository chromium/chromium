// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/desk_api/desk_api_extension_manager_factory.h"

#include "chrome/browser/chromeos/extensions/desk_api/desk_api_extension_manager.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"

namespace chromeos {

// static
DeskApiExtensionManager* DeskApiExtensionManagerFactory::GetForProfile(
    Profile* profile) {
  DCHECK(profile);
  return static_cast<DeskApiExtensionManager*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
DeskApiExtensionManagerFactory* DeskApiExtensionManagerFactory::GetInstance() {
  static base::NoDestructor<DeskApiExtensionManagerFactory> g_factory;
  return g_factory.get();
}

DeskApiExtensionManagerFactory::DeskApiExtensionManagerFactory()
    : ProfileKeyedServiceFactory(
          "DeskApiExtensionManager",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .WithGuest(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {}

DeskApiExtensionManagerFactory::~DeskApiExtensionManagerFactory() = default;

std::unique_ptr<KeyedService>
DeskApiExtensionManagerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  auto* const profile = Profile::FromBrowserContext(context);
  auto* const component_loader = ::extensions::ComponentLoader::Get(profile);
  return std::make_unique<DeskApiExtensionManager>(
      component_loader, profile,
      std::make_unique<DeskApiExtensionManager::Delegate>());
}

}  // namespace chromeos
