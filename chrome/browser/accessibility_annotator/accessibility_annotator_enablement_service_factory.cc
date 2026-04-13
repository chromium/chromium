// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility_annotator/accessibility_annotator_enablement_service_factory.h"

#include "base/strings/string_util.h"
#include "chrome/browser/account_settings/account_setting_service_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/subscription_eligibility/subscription_eligibility_service_factory.h"
#include "components/accessibility_annotator/core/accessibility_annotator_enablement_service_impl.h"
#include "components/accessibility_annotator/core/accessibility_annotator_features.h"
#include "components/accessibility_annotator/core/country_type.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/subscription_eligibility/subscription_eligibility_service.h"
#include "components/variations/service/variations_service.h"
#include "components/variations/service/variations_service_utils.h"

namespace {
// Return the latest country code from the chrome variation service.
// If the variation service is not available, an empty string is returned.
accessibility_annotator::GeoIpCountryCode GetCountryCodeFromVariations() {
  variations::VariationsService* variation_service =
      g_browser_process->variations_service();
  return accessibility_annotator::GeoIpCountryCode(
      variation_service
          ? base::ToUpperASCII(variation_service->GetLatestCountry())
          : std::string());
}
}  // namespace

// static
accessibility_annotator::AccessibilityAnnotatorEnablementService*
AccessibilityAnnotatorEnablementServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<
      accessibility_annotator::AccessibilityAnnotatorEnablementService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

// static
AccessibilityAnnotatorEnablementServiceFactory*
AccessibilityAnnotatorEnablementServiceFactory::GetInstance() {
  static base::NoDestructor<AccessibilityAnnotatorEnablementServiceFactory>
      instance;
  return instance.get();
}

AccessibilityAnnotatorEnablementServiceFactory::
    AccessibilityAnnotatorEnablementServiceFactory()
    : ProfileKeyedServiceFactory(
          "AccessibilityAnnotatorEnablementService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(AccountSettingServiceFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(subscription_eligibility::SubscriptionEligibilityServiceFactory::
                GetInstance());
}

AccessibilityAnnotatorEnablementServiceFactory::
    ~AccessibilityAnnotatorEnablementServiceFactory() = default;

std::unique_ptr<KeyedService> AccessibilityAnnotatorEnablementServiceFactory::
    BuildServiceInstanceForBrowserContext(
        content::BrowserContext* context) const {
  if (!base::FeatureList::IsEnabled(
          accessibility_annotator::features::kAccessibilityAnnotator)) {
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
      accessibility_annotator::AccessibilityAnnotatorEnablementServiceImpl>(
      account_settings_service, identity_manager,
      subscription_eligibility_service, profile->GetPrefs(),
      GetCountryCodeFromVariations());
}
