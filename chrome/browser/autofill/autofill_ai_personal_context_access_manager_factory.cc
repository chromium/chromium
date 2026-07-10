// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_ai_personal_context_access_manager_factory.h"

#include "base/feature_list.h"
#include "chrome/browser/personal_context/personal_context_eligibility_service_factory.h"
#include "chrome/browser/personal_context/personal_context_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/autofill/core/browser/network/autofill_ai/autofill_ai_personal_context_access_manager_impl.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/personal_context/core/personal_context_eligibility_service.h"
#include "components/personal_context/core/personal_context_service.h"

namespace autofill {

// static
AutofillAiPersonalContextAccessManager*
AutofillAiPersonalContextAccessManagerFactory::GetForProfile(Profile* profile) {
  return static_cast<AutofillAiPersonalContextAccessManager*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

// static
AutofillAiPersonalContextAccessManagerFactory*
AutofillAiPersonalContextAccessManagerFactory::GetInstance() {
  static base::NoDestructor<AutofillAiPersonalContextAccessManagerFactory>
      instance;
  return instance.get();
}

AutofillAiPersonalContextAccessManagerFactory::
    AutofillAiPersonalContextAccessManagerFactory()
    : ProfileKeyedServiceFactory(
          "AutofillAiPersonalContextAccessManager",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // Off-the-record profiles will default to
              // ProfileSelection::kNone.
              .Build()) {
  DependsOn(PersonalContextEligibilityServiceFactory::GetInstance());
  DependsOn(PersonalContextServiceFactory::GetInstance());
}

AutofillAiPersonalContextAccessManagerFactory::
    ~AutofillAiPersonalContextAccessManagerFactory() = default;

std::unique_ptr<KeyedService> AutofillAiPersonalContextAccessManagerFactory::
    BuildServiceInstanceForBrowserContext(
        content::BrowserContext* context) const {
  if (!base::FeatureList::IsEnabled(features::kAutofillAmbientAutofill)) {
    return nullptr;
  }

  Profile* profile = Profile::FromBrowserContext(context);
  personal_context::PersonalContextService* personal_context_service =
      PersonalContextServiceFactory::GetForProfile(profile);
  personal_context::PersonalContextEligibilityService*
      personal_context_eligibility_service =
          PersonalContextEligibilityServiceFactory::GetForProfile(profile);

  if (!personal_context_service || !personal_context_eligibility_service) {
    return nullptr;
  }

  return std::make_unique<AutofillAiPersonalContextAccessManagerImpl>(
      personal_context_service, personal_context_eligibility_service,
      profile->GetPrefs());
}

}  // namespace autofill
