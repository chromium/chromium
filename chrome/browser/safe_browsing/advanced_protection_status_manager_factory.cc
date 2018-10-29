// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/safe_browsing/advanced_protection_status_manager_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager.h"
#include "chrome/browser/signin/account_tracker_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/signin/core/browser/signin_manager.h"
#include "content/public/browser/browser_context.h"

namespace safe_browsing {

// static
AdvancedProtectionStatusManager*
AdvancedProtectionStatusManagerFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<AdvancedProtectionStatusManager*>(
      GetInstance()->GetServiceForBrowserContext(context, /* create= */ true));
}

// static
AdvancedProtectionStatusManagerFactory*
AdvancedProtectionStatusManagerFactory::GetInstance() {
  return base::Singleton<AdvancedProtectionStatusManagerFactory>::get();
}

AdvancedProtectionStatusManagerFactory::AdvancedProtectionStatusManagerFactory()
    : BrowserContextKeyedServiceFactory(
          "AdvancedProtectionStatusManager",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(AccountTrackerServiceFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
}

AdvancedProtectionStatusManagerFactory::
    ~AdvancedProtectionStatusManagerFactory() {}

KeyedService* AdvancedProtectionStatusManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new AdvancedProtectionStatusManager(
      Profile::FromBrowserContext(context));
}

bool AdvancedProtectionStatusManagerFactory::
    ServiceIsCreatedWithBrowserContext() const {
  return true;
}

}  // namespace safe_browsing
