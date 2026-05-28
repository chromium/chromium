// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/personal_context_access_manager_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "components/autofill/core/browser/network/autofill_ai/personal_context_access_manager_impl.h"

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
  // TODO(crbug.com/516721244): Add dependencies.
}

PersonalContextAccessManagerFactory::~PersonalContextAccessManagerFactory() =
    default;

std::unique_ptr<KeyedService>
PersonalContextAccessManagerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  // TODO(crbug.com/516721244): Add feature check and fetch dependencies using
  // other factories here before making the manager.

  return std::make_unique<autofill::PersonalContextAccessManagerImpl>();
}

}  // namespace autofill
