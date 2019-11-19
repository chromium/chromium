// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/kids_chrome_management/kids_chrome_management_client_factory.h"

#include "chrome/browser/supervised_user/kids_chrome_management/kids_chrome_management_client.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

// static
KidsChromeManagementClient*
KidsChromeManagementClientFactory::GetForBrowserContext(Profile* profile) {
  return static_cast<KidsChromeManagementClient*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
KidsChromeManagementClientFactory*
KidsChromeManagementClientFactory::GetInstance() {
  static base::NoDestructor<KidsChromeManagementClientFactory> factory;
  return factory.get();
}

KidsChromeManagementClientFactory::KidsChromeManagementClientFactory()
    : BrowserContextKeyedServiceFactory(
          "KidsChromeManagementClientFactory",
          BrowserContextDependencyManager::GetInstance()) {}

KidsChromeManagementClientFactory::~KidsChromeManagementClientFactory() =
    default;

KeyedService* KidsChromeManagementClientFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new KidsChromeManagementClient(static_cast<Profile*>(context));
}
