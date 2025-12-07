// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/tree_fixing/ax_tree_fixing_services_router_factory.h"

#include "chrome/browser/accessibility/tree_fixing/ax_tree_fixing_services_router.h"
#include "chrome/browser/accessibility/tree_fixing/pref_names.h"
#include "chrome/browser/profiles/profile.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/sync_preferences/pref_service_syncable.h"

namespace tree_fixing {

// static
AXTreeFixingServicesRouter* AXTreeFixingServicesRouterFactory::GetForProfile(
    Profile* profile) {
  return static_cast<AXTreeFixingServicesRouter*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

// static
AXTreeFixingServicesRouterFactory*
AXTreeFixingServicesRouterFactory::GetInstance() {
  static base::NoDestructor<AXTreeFixingServicesRouterFactory> instance;
  return instance.get();
}

AXTreeFixingServicesRouterFactory::AXTreeFixingServicesRouterFactory()
    : ProfileKeyedServiceFactory("AXTreeFixingService",
                                 ProfileSelections::BuildForRegularProfile()) {}

AXTreeFixingServicesRouterFactory::~AXTreeFixingServicesRouterFactory() =
    default;

bool AXTreeFixingServicesRouterFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

std::unique_ptr<KeyedService>
AXTreeFixingServicesRouterFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<AXTreeFixingServicesRouter>(
      Profile::FromBrowserContext(context));
}

void AXTreeFixingServicesRouterFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(
      prefs::kAccessibilityAXTreeFixingEnabled, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
}

}  // namespace tree_fixing
