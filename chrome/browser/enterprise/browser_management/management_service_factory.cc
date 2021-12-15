// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/browser_management/management_service_factory.h"

#include "base/memory/singleton.h"
#include "base/no_destructor.h"
#include "chrome/browser/enterprise/browser_management/browser_management_service.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/browsing_data/core/features.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/policy/core/common/management/platform_management_service.h"
#include "content/public/browser/browser_context.h"
#include "extensions/buildflags/buildflags.h"

namespace policy {

// static
ManagementServiceFactory* ManagementServiceFactory::GetInstance() {
  static base::NoDestructor<ManagementServiceFactory> instance;
  return instance.get();
}

// static
ManagementService* ManagementServiceFactory::GetForPlatform() {
  return &(GetInstance()->platform_management_service_);
}

// static
ManagementService* ManagementServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<BrowserManagementService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

ManagementServiceFactory::ManagementServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "EnterpriseManagementService",
          BrowserContextDependencyManager::GetInstance()) {}

ManagementServiceFactory::~ManagementServiceFactory() = default;

content::BrowserContext* ManagementServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}

KeyedService* ManagementServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new BrowserManagementService(Profile::FromBrowserContext(context));
}

}  // namespace policy
