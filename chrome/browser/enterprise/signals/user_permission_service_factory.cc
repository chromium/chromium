// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/signals/user_permission_service_factory.h"

#include <memory>

#include "base/no_destructor.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/enterprise/connectors/device_trust/device_trust_connector_service.h"
#include "chrome/browser/enterprise/connectors/device_trust/device_trust_connector_service_factory.h"
#include "chrome/browser/enterprise/signals/user_delegate_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/device_signals/core/browser/user_delegate.h"
#include "components/device_signals/core/browser/user_permission_service.h"
#include "components/device_signals/core/browser/user_permission_service_impl.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/browser_context.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "components/device_signals/core/browser/ash/user_permission_service_ash.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace enterprise_signals {

// static
UserPermissionServiceFactory* UserPermissionServiceFactory::GetInstance() {
  static base::NoDestructor<UserPermissionServiceFactory> instance;
  return instance.get();
}

// static
device_signals::UserPermissionService*
UserPermissionServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<device_signals::UserPermissionService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

UserPermissionServiceFactory::UserPermissionServiceFactory()
    : ProfileKeyedServiceFactory(
          "UserPermissionService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOwnInstance)
              .Build()) {
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(policy::ManagementServiceFactory::GetInstance());
  DependsOn(
      enterprise_connectors::DeviceTrustConnectorServiceFactory::GetInstance());
}

UserPermissionServiceFactory::~UserPermissionServiceFactory() = default;

KeyedService* UserPermissionServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  auto* profile = Profile::FromBrowserContext(context);

  auto* device_trust_connector_service =
      enterprise_connectors::DeviceTrustConnectorServiceFactory::GetForProfile(
          profile);

  if (!device_trust_connector_service) {
    // Unsupported configuration (e.g. CrOS login Profile supported, but not
    // incognito).
    return nullptr;
  }

  auto* management_service =
      policy::ManagementServiceFactory::GetForProfile(profile);

  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);

  auto user_delegate = std::make_unique<UserDelegateImpl>(
      profile, identity_manager, device_trust_connector_service);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  auto* user_permission_service = new device_signals::UserPermissionServiceAsh(
      management_service, std::move(user_delegate), profile->GetPrefs());
#else
  auto* user_permission_service = new device_signals::UserPermissionServiceImpl(
      management_service, std::move(user_delegate), profile->GetPrefs());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  return user_permission_service;
}

}  // namespace enterprise_signals
