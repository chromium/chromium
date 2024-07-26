// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/cert_provisioning/cert_provisioning_scheduler_user_service.h"

#include "base/no_destructor.h"
#include "chrome/browser/ash/platform_keys/platform_keys_service_factory.h"
#include "chrome/browser/invalidation/profile_invalidation_provider_factory.h"
#include "chrome/browser/policy/cloud/user_fm_registration_token_uploader_factory.h"
#include "chrome/browser/policy/profile_policy_connector.h"

namespace ash {
namespace cert_provisioning {

// ================== CertProvisioningSchedulerUserService =====================

CertProvisioningSchedulerUserService::CertProvisioningSchedulerUserService(
    Profile* profile)
    : scheduler_(
          CertProvisioningSchedulerImpl::CreateUserCertProvisioningScheduler(
              profile)) {}

CertProvisioningSchedulerUserService::~CertProvisioningSchedulerUserService() =
    default;

void CertProvisioningSchedulerUserService::Shutdown() {
  // The scheduler uses invalidations and depends on
  // `ProfileInvalidationProvider`. Invalidation service is destroyed on
  // Shutdown call and expects all invalidators to be unregistered before that.
  // Destroy `scheduler_` and its invalidators on service's shutdown to fulfill
  // this requirement.
  scheduler_.reset();
}

// ================ CertProvisioningSchedulerUserServiceFactory ================

// static
CertProvisioningSchedulerUserService*
CertProvisioningSchedulerUserServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<CertProvisioningSchedulerUserService*>(
      CertProvisioningSchedulerUserServiceFactory::GetInstance()
          ->GetServiceForBrowserContext(profile, /*create=*/true));
}

// static
CertProvisioningSchedulerUserServiceFactory*
CertProvisioningSchedulerUserServiceFactory::GetInstance() {
  static base::NoDestructor<CertProvisioningSchedulerUserServiceFactory>
      factory;
  return factory.get();
}

CertProvisioningSchedulerUserServiceFactory::
    CertProvisioningSchedulerUserServiceFactory()
    : ProfileKeyedServiceFactory("CertProvisioningSchedulerUserService",
                                 ProfileSelections::Builder()
                                     .WithGuest(ProfileSelection::kOriginalOnly)
                                     .WithAshInternals(ProfileSelection::kNone)
                                     .Build()) {
  DependsOn(platform_keys::PlatformKeysServiceFactory::GetInstance());
  DependsOn(invalidation::ProfileInvalidationProviderFactory::GetInstance());
  DependsOn(policy::UserFmRegistrationTokenUploaderFactory::GetInstance());
}

bool CertProvisioningSchedulerUserServiceFactory::
    ServiceIsCreatedWithBrowserContext() const {
  return true;
}

std::unique_ptr<KeyedService> CertProvisioningSchedulerUserServiceFactory::
    BuildServiceInstanceForBrowserContext(
        content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  if (!profile || !profile->GetProfilePolicyConnector()->IsManaged()) {
    return nullptr;
  }

  return std::make_unique<CertProvisioningSchedulerUserService>(profile);
}

}  // namespace cert_provisioning
}  // namespace ash
