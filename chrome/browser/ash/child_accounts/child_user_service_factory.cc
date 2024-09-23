// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/child_accounts/child_user_service_factory.h"

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/child_accounts/child_user_service.h"

namespace ash {

// static
ChildUserService* ChildUserServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<ChildUserService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
ChildUserServiceFactory* ChildUserServiceFactory::GetInstance() {
  static base::NoDestructor<ChildUserServiceFactory> factory;
  return factory.get();
}

ChildUserServiceFactory::ChildUserServiceFactory()
    : ProfileKeyedServiceFactory(
          "ChildUserServiceFactory",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(apps::AppServiceProxyFactory::GetInstance());
}

ChildUserServiceFactory::~ChildUserServiceFactory() = default;

std::unique_ptr<KeyedService>
ChildUserServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<ChildUserService>(context);
}

}  // namespace ash
