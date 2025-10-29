// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/chrome_policy_blocklist_service_factory.h"

#include <memory>

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/policy/core/browser/url_blocklist_manager.h"
#include "components/policy/core/browser/url_list/policy_blocklist_service.h"
#include "components/policy/core/common/policy_pref_names.h"

// TODO(crbug.com/454904366): Remove all dependencies on this factory that
// reside inside //chrome/browser/ash/
// static
PolicyBlocklistService* ChromePolicyBlocklistServiceFactory::GetForProfile(
    Profile* profile) {

  return static_cast<PolicyBlocklistService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
ChromePolicyBlocklistServiceFactory*
ChromePolicyBlocklistServiceFactory::GetInstance() {
  static base::NoDestructor<ChromePolicyBlocklistServiceFactory> instance;
  return instance.get();
}

ChromePolicyBlocklistServiceFactory::ChromePolicyBlocklistServiceFactory()
    : ProfileKeyedServiceFactory(
          "ChromePolicyBlocklistService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              .WithGuest(ProfileSelection::kOwnInstance)
              // System profile is needed, as the service is called in
              // navigation which is created in Profile Picker.
              .WithSystem(ProfileSelection::kOwnInstance)
              // AshInternals profile is needed, as the service is called in
              // navigation which is created in ChromeOS sign-in.
              .WithAshInternals(ProfileSelection::kOwnInstance)
              .Build()) {}

ChromePolicyBlocklistServiceFactory::~ChromePolicyBlocklistServiceFactory() =
    default;

std::unique_ptr<KeyedService>
ChromePolicyBlocklistServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  PrefService* pref_service = Profile::FromBrowserContext(context)->GetPrefs();
  auto url_blocklist_manager = std::make_unique<policy::URLBlocklistManager>(
      pref_service, policy::policy_prefs::kUrlBlocklist,
      policy::policy_prefs::kUrlAllowlist);
  return std::make_unique<PolicyBlocklistService>(
      std::move(url_blocklist_manager), pref_service);
}
