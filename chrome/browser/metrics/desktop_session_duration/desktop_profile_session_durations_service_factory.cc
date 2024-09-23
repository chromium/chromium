// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/desktop_session_duration/desktop_profile_session_durations_service_factory.h"

#include "chrome/browser/metrics/desktop_session_duration/desktop_profile_session_durations_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
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
  static base::NoDestructor<DesktopProfileSessionDurationsServiceFactory>
      instance;
  return instance.get();
}

DesktopProfileSessionDurationsServiceFactory::
    DesktopProfileSessionDurationsServiceFactory()
    : ProfileKeyedServiceFactory(
          "DesktopProfileSessionDurationsService",
          // Guest and system profiles are excluded from session metrics.
          //
          // Session time in incognito is counted towards the session time in
          // the regular profile. That means that for a user that is signed in
          // and syncing in their regular profile and that is browsing in
          // incognito profile, Chromium will record the session time as being
          // signed in and syncing.
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kRedirectedToOriginal)
              .Build()) {
  DependsOn(SyncServiceFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
}

DesktopProfileSessionDurationsServiceFactory::
    ~DesktopProfileSessionDurationsServiceFactory() = default;

std::unique_ptr<KeyedService> DesktopProfileSessionDurationsServiceFactory::
    BuildServiceInstanceForBrowserContext(
        content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);

// On Ash and Lacros IsGuestSession and IsRegularProfile() are not mutually
// exclusive, which breaks `ProfileKeyedServiceFactory` logic. THerefore the
// below check is still needed despite the proper filter already set.
#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
  if (profile->IsGuestSession())
    return nullptr;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)

  DCHECK(!profile->IsSystemProfile());
  DCHECK(!profile->IsGuestSession());

  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile);
  DesktopSessionDurationTracker* tracker = DesktopSessionDurationTracker::Get();
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  return std::make_unique<DesktopProfileSessionDurationsService>(
      profile->GetPrefs(), sync_service, identity_manager, tracker);
}

}  // namespace metrics
