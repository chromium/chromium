// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/personal_context_access_manager_factory.h"

#include "base/feature_list.h"
#include "chrome/browser/personal_context/personal_context_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/autofill/core/browser/network/autofill_ai/personal_context_access_manager_impl.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/personal_context/core/personal_context_service.h"

namespace autofill {

// static
autofill::PersonalContextAccessManager*
PersonalContextAccessManagerFactory::GetForProfile(Profile* profile) {
  return static_cast<autofill::PersonalContextAccessManager*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

// static
PersonalContextAccessManagerFactory*
PersonalContextAccessManagerFactory::GetInstance() {
  static base::NoDestructor<PersonalContextAccessManagerFactory> instance;
  return instance.get();
}

PersonalContextAccessManagerFactory::PersonalContextAccessManagerFactory()
    : ProfileKeyedServiceFactory(
          "PersonalContextAccessManager",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // Off-the-record profiles will default to
              // ProfileSelection::kNone.
              .Build()) {
  DependsOn(PersonalContextServiceFactory::GetInstance());
}

PersonalContextAccessManagerFactory::~PersonalContextAccessManagerFactory() =
    default;

std::unique_ptr<KeyedService>
PersonalContextAccessManagerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!base::FeatureList::IsEnabled(
          autofill::features::kAutofillAmbientAutofill)) {
    return nullptr;
  }

  Profile* profile = Profile::FromBrowserContext(context);
  personal_context::PersonalContextService* personal_context_service =
      PersonalContextServiceFactory::GetForProfile(profile);
  if (!personal_context_service) {
    return nullptr;
  }

  return std::make_unique<autofill::PersonalContextAccessManagerImpl>(
      personal_context_service);
}

}  // namespace autofill
