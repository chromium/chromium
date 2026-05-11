// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/personal_context/personal_context_enablement_service_factory.h"

#include "base/strings/string_util.h"
#include "chrome/browser/account_settings/account_setting_service_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/subscription_eligibility/subscription_eligibility_service_factory.h"
#include "components/personal_context/core/country_type.h"
#include "components/personal_context/core/personal_context_enablement_service_impl.h"
#include "components/personal_context/core/personal_context_features.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/subscription_eligibility/subscription_eligibility_service.h"
#include "components/variations/service/variations_service.h"
#include "components/variations/service/variations_service_utils.h"

namespace {
// Return the latest country code from the chrome variation service.
// If the variation service is not available, an empty string is returned.
personal_context::GeoIpCountryCode GetCountryCodeFromVariations() {
  variations::VariationsService* variation_service =
      g_browser_process->variations_service();
  return personal_context::GeoIpCountryCode(
      variation_service
          ? base::ToUpperASCII(variation_service->GetLatestCountry())
          : std::string());
}
}  // namespace

// static
personal_context::PersonalContextEnablementService*
PersonalContextEnablementServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<personal_context::PersonalContextEnablementService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

// static
PersonalContextEnablementServiceFactory*
PersonalContextEnablementServiceFactory::GetInstance() {
  static base::NoDestructor<PersonalContextEnablementServiceFactory> instance;
  return instance.get();
}

PersonalContextEnablementServiceFactory::
    PersonalContextEnablementServiceFactory()
    : ProfileKeyedServiceFactory(
          "PersonalContextEnablementService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(AccountSettingServiceFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(subscription_eligibility::SubscriptionEligibilityServiceFactory::
                GetInstance());
}

PersonalContextEnablementServiceFactory::
    ~PersonalContextEnablementServiceFactory() = default;

std::unique_ptr<KeyedService>
PersonalContextEnablementServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!base::FeatureList::IsEnabled(
          personal_context::features::kPersonalContext)) {
    return nullptr;
  }

  Profile* profile = Profile::FromBrowserContext(context);

  account_settings::AccountSettingService* account_settings_service =
      AccountSettingServiceFactory::GetForProfile(
          profile->GetOriginalProfile());
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile->GetOriginalProfile());
  subscription_eligibility::SubscriptionEligibilityService*
      subscription_eligibility_service =
          subscription_eligibility::SubscriptionEligibilityServiceFactory::
              GetForProfile(profile->GetOriginalProfile());
  return std::make_unique<
      personal_context::PersonalContextEnablementServiceImpl>(
      account_settings_service, identity_manager,
      subscription_eligibility_service, profile->GetPrefs(),
      GetCountryCodeFromVariations());
}
