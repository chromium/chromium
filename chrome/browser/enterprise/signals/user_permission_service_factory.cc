// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/signals/user_permission_service_factory.h"

#include <memory>

#include "base/memory/singleton.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
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

namespace enterprise_signals {

// static
UserPermissionServiceFactory* UserPermissionServiceFactory::GetInstance() {
  return base::Singleton<UserPermissionServiceFactory>::get();
}

// static
device_signals::UserPermissionService*
UserPermissionServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<device_signals::UserPermissionService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

UserPermissionServiceFactory::UserPermissionServiceFactory()
    : ProfileKeyedServiceFactory("UserPermissionService") {
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(policy::ManagementServiceFactory::GetInstance());
}

UserPermissionServiceFactory::~UserPermissionServiceFactory() = default;

KeyedService* UserPermissionServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  auto* profile = Profile::FromBrowserContext(context);

  auto* management_service =
      policy::ManagementServiceFactory::GetForProfile(profile);

  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);

  return new device_signals::UserPermissionServiceImpl(
      management_service,
      std::make_unique<UserDelegateImpl>(profile, identity_manager));
}

}  // namespace enterprise_signals
