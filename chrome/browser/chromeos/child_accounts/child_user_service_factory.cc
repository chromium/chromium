// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/child_accounts/child_user_service_factory.h"

#include "chrome/browser/chromeos/child_accounts/child_user_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace chromeos {

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
    : BrowserContextKeyedServiceFactory(
          "ChildUserServiceFactory",
          BrowserContextDependencyManager::GetInstance()) {}

ChildUserServiceFactory::~ChildUserServiceFactory() = default;

KeyedService* ChildUserServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new ChildUserService(context);
}

}  // namespace chromeos
