// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/child_accounts/child_user_service_factory.h"

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
    : ProfileKeyedServiceFactory("ChildUserServiceFactory") {
  DependsOn(apps::AppServiceProxyFactory::GetInstance());
}

ChildUserServiceFactory::~ChildUserServiceFactory() = default;

KeyedService* ChildUserServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new ChildUserService(context);
}

}  // namespace ash
