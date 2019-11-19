// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/launch_service/launch_service_factory.h"

#include "chrome/browser/apps/launch_service/launch_service.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace apps {

// static
LaunchService* LaunchServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<LaunchService*>(
      LaunchServiceFactory::GetInstance()->GetServiceForBrowserContext(
          profile, true /* create */));
}

// static
LaunchServiceFactory* LaunchServiceFactory::GetInstance() {
  return base::Singleton<LaunchServiceFactory>::get();
}

LaunchServiceFactory::LaunchServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "LaunchService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(extensions::ExtensionSystemFactory::GetInstance());
}

LaunchServiceFactory::~LaunchServiceFactory() = default;

KeyedService* LaunchServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new LaunchService(Profile::FromBrowserContext(context));
}

bool LaunchServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

content::BrowserContext* LaunchServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  Profile* const profile = Profile::FromBrowserContext(context);

  if (profile->IsGuestSession()) {
    // In guest mode, only off-the-record browsers may be opened.
    return profile->GetOffTheRecordProfile();
  }

  // Do not create secondary instance for incognito WebContents with
  // off-the-record profiles for non-guest sessions. Redirect to the instance
  // from the original profile part.
  return profile->GetOriginalProfile();
}

}  // namespace apps
