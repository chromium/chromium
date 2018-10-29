// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/cloud_print/privet_notifications_factory.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/printing/cloud_print/privet_notifications.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace cloud_print {

PrivetNotificationServiceFactory*
PrivetNotificationServiceFactory::GetInstance() {
  return base::Singleton<PrivetNotificationServiceFactory>::get();
}

PrivetNotificationServiceFactory::PrivetNotificationServiceFactory()
    : BrowserContextKeyedServiceFactory(
        "PrivetNotificationService",
        BrowserContextDependencyManager::GetInstance()) {
  DependsOn(IdentityManagerFactory::GetInstance());
}

PrivetNotificationServiceFactory::~PrivetNotificationServiceFactory() {
}

KeyedService* PrivetNotificationServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return new PrivetNotificationService(profile);
}

bool
PrivetNotificationServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return PrivetNotificationService::IsEnabled();
}

bool PrivetNotificationServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace cloud_print
