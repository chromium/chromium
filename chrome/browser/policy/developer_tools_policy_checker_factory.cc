// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/developer_tools_policy_checker_factory.h"

#include "chrome/browser/policy/developer_tools_policy_checker.h"
#include "chrome/browser/policy/developer_tools_policy_handler.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "content/public/browser/browser_context.h"

namespace policy {

// static
DeveloperToolsPolicyChecker*
DeveloperToolsPolicyCheckerFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<DeveloperToolsPolicyChecker*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
DeveloperToolsPolicyCheckerFactory*
DeveloperToolsPolicyCheckerFactory::GetInstance() {
  static base::NoDestructor<DeveloperToolsPolicyCheckerFactory> instance;
  return instance.get();
}

DeveloperToolsPolicyCheckerFactory::DeveloperToolsPolicyCheckerFactory()
    : ProfileKeyedServiceFactory(
          "DeveloperToolsPolicyChecker",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              .WithGuest(ProfileSelection::kOwnInstance)
              .WithSystem(ProfileSelection::kNone)
              .WithAshInternals(ProfileSelection::kNone)
              .Build()) {}

DeveloperToolsPolicyCheckerFactory::~DeveloperToolsPolicyCheckerFactory() =
    default;

std::unique_ptr<KeyedService>
DeveloperToolsPolicyCheckerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<DeveloperToolsPolicyChecker>(
      Profile::FromBrowserContext(context)->GetPrefs());
}

void DeveloperToolsPolicyCheckerFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  // Prefs are registered here on all platforms, but used only on Desktop.
  // TODO(crbug.com/442892562) Add implementation for mobile.
  registry->RegisterIntegerPref(
      prefs::kDevToolsAvailability,
      static_cast<int>(DeveloperToolsPolicyHandler::Availability::
                           kDisallowedForForceInstalledExtensions));
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || \
    BUILDFLAG(IS_CHROMEOS)
  registry->RegisterListPref(prefs::kDeveloperToolsAvailabilityAllowlist);
  registry->RegisterListPref(prefs::kDeveloperToolsAvailabilityBlocklist);
#endif
}

}  // namespace policy
