// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/contact_center_insights/contact_center_insights_extension_manager_factory.h"

#include <utility>

#include "base/check.h"
#include "chrome/browser/chromeos/extensions/contact_center_insights/contact_center_insights_extension_manager.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "extensions/browser/extension_system.h"

namespace chromeos {

// static
ContactCenterInsightsExtensionManager*
ContactCenterInsightsExtensionManagerFactory::GetForProfile(Profile* profile) {
  CHECK(profile);
  return static_cast<ContactCenterInsightsExtensionManager*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
ContactCenterInsightsExtensionManagerFactory*
ContactCenterInsightsExtensionManagerFactory::GetInstance() {
  static base::NoDestructor<ContactCenterInsightsExtensionManagerFactory>
      g_factory;
  return g_factory.get();
}

ContactCenterInsightsExtensionManagerFactory::
    ContactCenterInsightsExtensionManagerFactory()
    : ProfileKeyedServiceFactory(
          "ContactCenterInsightsExtensionManager",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {}

ContactCenterInsightsExtensionManagerFactory::
    ~ContactCenterInsightsExtensionManagerFactory() = default;

KeyedService*
ContactCenterInsightsExtensionManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  auto* const profile = Profile::FromBrowserContext(context);
  auto* const component_loader = ::extensions::ExtensionSystem::Get(profile)
                                     ->extension_service()
                                     ->component_loader();
  return new ContactCenterInsightsExtensionManager(
      component_loader, profile,
      std::make_unique<ContactCenterInsightsExtensionManager::Delegate>());
}

}  // namespace chromeos
