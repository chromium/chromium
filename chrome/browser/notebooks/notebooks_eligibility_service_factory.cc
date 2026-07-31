// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notebooks/notebooks_eligibility_service_factory.h"

#include <memory>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/notebooks/internal/notebooks_eligibility_service_impl.h"
#include "components/notebooks/public/notebooks_eligibility_service.h"
#include "content/public/browser/browser_context.h"

namespace notebooks {

// static
NotebooksEligibilityService* NotebooksEligibilityServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<NotebooksEligibilityService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
NotebooksEligibilityServiceFactory*
NotebooksEligibilityServiceFactory::GetInstance() {
  static base::NoDestructor<NotebooksEligibilityServiceFactory> instance;
  return instance.get();
}

NotebooksEligibilityServiceFactory::NotebooksEligibilityServiceFactory()
    : ProfileKeyedServiceFactory(
          "NotebooksEligibilityService",
          ProfileSelections::BuildForRegularAndIncognito()) {
  DependsOn(IdentityManagerFactory::GetInstance());
}

NotebooksEligibilityServiceFactory::~NotebooksEligibilityServiceFactory() =
    default;

std::unique_ptr<KeyedService>
NotebooksEligibilityServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  // Note: Guest and system profiles are excluded by ProfileSelections above
  // and will never reach this factory method.
  bool is_profile_eligible = !profile->IsOffTheRecord();
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  return std::make_unique<NotebooksEligibilityServiceImpl>(is_profile_eligible,
                                                           identity_manager);
}

}  // namespace notebooks
