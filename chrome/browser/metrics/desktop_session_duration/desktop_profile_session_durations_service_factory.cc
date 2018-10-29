// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/desktop_session_duration/desktop_profile_session_durations_service_factory.h"

#include "chrome/browser/metrics/desktop_session_duration/desktop_profile_session_durations_service.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/gaia_cookie_manager_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "content/public/browser/browser_context.h"

namespace metrics {

// static
DesktopProfileSessionDurationsService*
DesktopProfileSessionDurationsServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<DesktopProfileSessionDurationsService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
DesktopProfileSessionDurationsServiceFactory*
DesktopProfileSessionDurationsServiceFactory::GetInstance() {
  return base::Singleton<DesktopProfileSessionDurationsServiceFactory>::get();
}

DesktopProfileSessionDurationsServiceFactory::
    DesktopProfileSessionDurationsServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "DesktopProfileSessionDurations",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(GaiaCookieManagerServiceFactory::GetInstance());
  DependsOn(ProfileSyncServiceFactory::GetInstance());
  DependsOn(ProfileOAuth2TokenServiceFactory::GetInstance());
}

DesktopProfileSessionDurationsServiceFactory::
    ~DesktopProfileSessionDurationsServiceFactory() = default;

KeyedService*
DesktopProfileSessionDurationsServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  browser_sync::ProfileSyncService* sync_service =
      ProfileSyncServiceFactory::GetForProfile(profile);
  GaiaCookieManagerService* cookie_manager =
      GaiaCookieManagerServiceFactory::GetForProfile(profile);
  DesktopSessionDurationTracker* tracker = DesktopSessionDurationTracker::Get();
  identity::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  return new DesktopProfileSessionDurationsService(
      sync_service, identity_manager, cookie_manager, tracker);
}

content::BrowserContext*
DesktopProfileSessionDurationsServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

}  // namespace metrics
