// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud/user_cloud_policy_invalidator_factory.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/invalidation/profile_invalidation_provider_factory.h"
#include "chrome/browser/policy/cloud/user_cloud_policy_invalidator.h"
#include "chrome/browser/policy/cloud/user_fm_registration_token_uploader_factory.h"
#include "chrome/browser/profiles/profile.h"
#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/policy/core/user_cloud_policy_manager_ash.h"
#else
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#endif

namespace policy {

// static
UserCloudPolicyInvalidatorFactory*
    UserCloudPolicyInvalidatorFactory::GetInstance() {
  static base::NoDestructor<UserCloudPolicyInvalidatorFactory> instance;
  return instance.get();
}

UserCloudPolicyInvalidatorFactory::UserCloudPolicyInvalidatorFactory()
    : ProfileKeyedServiceFactory(
          "UserCloudPolicyInvalidator",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(invalidation::ProfileInvalidationProviderFactory::GetInstance());
  DependsOn(UserFmRegistrationTokenUploaderFactory::GetInstance());
}

UserCloudPolicyInvalidatorFactory::~UserCloudPolicyInvalidatorFactory() =
    default;

std::unique_ptr<KeyedService>
UserCloudPolicyInvalidatorFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = static_cast<Profile*>(context);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  CloudPolicyManager* policy_manager = profile->GetUserCloudPolicyManagerAsh();
#else
  CloudPolicyManager* policy_manager = profile->GetUserCloudPolicyManager();
#endif
  if (!policy_manager)
    return nullptr;

  return std::make_unique<UserCloudPolicyInvalidator>(profile, policy_manager);
}

bool UserCloudPolicyInvalidatorFactory::
ServiceIsCreatedWithBrowserContext() const {
  // Must be automatically created to enable user policy invalidations.
  return true;
}

bool UserCloudPolicyInvalidatorFactory::ServiceIsNULLWhileTesting() const {
  // Not used in tests.
  return true;
}

}  // namespace policy
