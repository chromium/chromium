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
  static base::NoDestructor<SessionDataServiceFactory> instance;
  return instance.get();
}

SessionDataServiceFactory::SessionDataServiceFactory()
    : ProfileKeyedServiceFactory(
          "SessionDataService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(HostContentSettingsMapFactory::GetInstance());
  DependsOn(CookieSettingsFactory::GetInstance());
}

SessionDataServiceFactory::~SessionDataServiceFactory() = default;

std::unique_ptr<KeyedService>
SessionDataServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* browser_context) const {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  auto deleter = std::make_unique<SessionDataDeleter>(profile);
  return std::make_unique<SessionDataService>(profile, std::move(deleter));
}

bool SessionDataServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

bool SessionDataServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
