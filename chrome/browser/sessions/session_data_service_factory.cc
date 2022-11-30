// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/session_data_service_factory.h"

#include <memory>

#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_data_deleter.h"
#include "chrome/browser/sessions/session_data_service.h"

// static
SessionDataService* SessionDataServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<SessionDataService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

SessionDataServiceFactory* SessionDataServiceFactory::GetInstance() {
  return base::Singleton<SessionDataServiceFactory>::get();
}

SessionDataServiceFactory::SessionDataServiceFactory()
    : ProfileKeyedServiceFactory("SessionDataService") {
  DependsOn(HostContentSettingsMapFactory::GetInstance());
  DependsOn(CookieSettingsFactory::GetInstance());
}

SessionDataServiceFactory::~SessionDataServiceFactory() = default;

KeyedService* SessionDataServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* browser_context) const {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  auto deleter = std::make_unique<SessionDataDeleter>(profile);
  return new SessionDataService(profile, std::move(deleter));
}

bool SessionDataServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

bool SessionDataServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
