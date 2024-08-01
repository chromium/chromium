// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud/user_fm_registration_token_uploader_factory.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/invalidation/profile_invalidation_provider_factory.h"
#include "chrome/browser/policy/cloud/user_fm_registration_token_uploader.h"
#include "chrome/browser/profiles/profile.h"
#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/policy/core/user_cloud_policy_manager_ash.h"
#else
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#endif

namespace policy {

namespace {

auto* GetCloudPolicyManager(Profile* profile) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return profile->GetUserCloudPolicyManagerAsh();
#else
  return profile->GetUserCloudPolicyManager();
#endif
}

}  // namespace

// static
UserFmRegistrationTokenUploaderFactory*
UserFmRegistrationTokenUploaderFactory::GetInstance() {
  static base::NoDestructor<UserFmRegistrationTokenUploaderFactory> instance;
  return instance.get();
}

UserFmRegistrationTokenUploaderFactory::UserFmRegistrationTokenUploaderFactory()
    : ProfileKeyedServiceFactory(
          "UserFmRegistrationTokenUploader",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(invalidation::ProfileInvalidationProviderFactory::GetInstance());
}

UserFmRegistrationTokenUploaderFactory::
    ~UserFmRegistrationTokenUploaderFactory() = default;

std::unique_ptr<KeyedService>
UserFmRegistrationTokenUploaderFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = static_cast<Profile*>(context);
  auto* policy_manager = GetCloudPolicyManager(profile);

  if (!policy_manager) {
    return nullptr;
  }

  return std::make_unique<UserFmRegistrationTokenUploader>(profile,
                                                           policy_manager);
}

bool UserFmRegistrationTokenUploaderFactory::
    ServiceIsCreatedWithBrowserContext() const {
  // Must be automatically created to enable user policy invalidations.
  return true;
}

bool UserFmRegistrationTokenUploaderFactory::ServiceIsNULLWhileTesting() const {
  // Not used in tests.
  return true;
}

}  // namespace policy
