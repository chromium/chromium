// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/captive_portal/captive_portal_service_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "components/captive_portal/content/captive_portal_service.h"

// static
captive_portal::CaptivePortalService*
CaptivePortalServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<captive_portal::CaptivePortalService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
CaptivePortalServiceFactory* CaptivePortalServiceFactory::GetInstance() {
  static base::NoDestructor<CaptivePortalServiceFactory> instance;
  return instance.get();
}

CaptivePortalServiceFactory::CaptivePortalServiceFactory()
    : ProfileKeyedServiceFactory(
          "captive_portal::CaptivePortalService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOwnInstance)
              .Build()) {}

CaptivePortalServiceFactory::~CaptivePortalServiceFactory() = default;

std::unique_ptr<KeyedService>
CaptivePortalServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* profile) const {
  return std::make_unique<captive_portal::CaptivePortalService>(
      profile, static_cast<Profile*>(profile)->GetPrefs());
}
