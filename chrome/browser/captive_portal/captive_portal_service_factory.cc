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
  return base::Singleton<CaptivePortalServiceFactory>::get();
}

CaptivePortalServiceFactory::CaptivePortalServiceFactory()
    : ProfileKeyedServiceFactory(
          "captive_portal::CaptivePortalService",
          ProfileSelections::BuildForRegularAndIncognito()) {}

CaptivePortalServiceFactory::~CaptivePortalServiceFactory() {
}

KeyedService* CaptivePortalServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return new captive_portal::CaptivePortalService(
      profile, static_cast<Profile*>(profile)->GetPrefs());
}
