// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/desktop_session_duration/desktop_profile_session_durations_service_factory.h"

#include "chrome/browser/metrics/desktop_session_duration/desktop_profile_session_durations_service.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
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
          "DesktopProfileSessionDurationsService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(ProfileSyncServiceFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
}

DesktopProfileSessionDurationsServiceFactory::
    ~DesktopProfileSessionDurationsServiceFactory() = default;

KeyedService*
DesktopProfileSessionDurationsServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  syncer::SyncService* sync_service =
      ProfileSyncServiceFactory::GetForProfile(profile);
  DesktopSessionDurationTracker* tracker = DesktopSessionDurationTracker::Get();
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  return new DesktopProfileSessionDurationsService(sync_service,
                                                   identity_manager, tracker);
}

content::BrowserContext*
DesktopProfileSessionDurationsServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // Session time in incognito is counted towards the session time in the
  // regular profile. That means that for a user that is signed in and syncing
  // in their regular profile and that is browsing in incognito profile,
  // Chromium will record the session time as being signed in and syncing.
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

}  // namespace metrics
