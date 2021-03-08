// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/metrics/android_incognito_session_durations_service_factory.h"

#include "build/build_config.h"
#include "chrome/browser/android/metrics/android_incognito_session_durations_service.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

// static
AndroidIncognitoSessionDurationsService*
AndroidIncognitoSessionDurationsServiceFactory::GetForActiveUserProfile() {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  DCHECK(profile);
  if (!profile->HasPrimaryOTRProfile())
    return nullptr;
  return AndroidIncognitoSessionDurationsServiceFactory::GetForProfile(
      profile->GetPrimaryOTRProfile());
}

// static
AndroidIncognitoSessionDurationsService*
AndroidIncognitoSessionDurationsServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<AndroidIncognitoSessionDurationsService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
AndroidIncognitoSessionDurationsServiceFactory*
AndroidIncognitoSessionDurationsServiceFactory::GetInstance() {
  return base::Singleton<AndroidIncognitoSessionDurationsServiceFactory>::get();
}

AndroidIncognitoSessionDurationsServiceFactory::
    AndroidIncognitoSessionDurationsServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "AndroidIncognitoSessionDurationsService",
          BrowserContextDependencyManager::GetInstance()) {}

AndroidIncognitoSessionDurationsServiceFactory::
    ~AndroidIncognitoSessionDurationsServiceFactory() = default;

KeyedService*
AndroidIncognitoSessionDurationsServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new AndroidIncognitoSessionDurationsService();
}

content::BrowserContext*
AndroidIncognitoSessionDurationsServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  if (!context->IsOffTheRecord())
    return nullptr;
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}

bool AndroidIncognitoSessionDurationsServiceFactory::ServiceIsNULLWhileTesting()
    const {
  return true;
}

bool AndroidIncognitoSessionDurationsServiceFactory::
    ServiceIsCreatedWithBrowserContext() const {
#if defined(OS_ANDROID)
  return true;
#else
  return false;
#endif
}
