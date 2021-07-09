// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing_hub/sharing_hub_service_factory.h"

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sharing_hub/sharing_hub_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace sharing_hub {

// static
SharingHubService* SharingHubServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<SharingHubService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
SharingHubServiceFactory* SharingHubServiceFactory::GetInstance() {
  return base::Singleton<SharingHubServiceFactory>::get();
}

SharingHubServiceFactory::SharingHubServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "SharingHubService",
          BrowserContextDependencyManager::GetInstance()) {}

SharingHubServiceFactory::~SharingHubServiceFactory() = default;

KeyedService* SharingHubServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new SharingHubService(context);
}

}  // namespace sharing_hub
