// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/background/ntp_custom_background_service_factory.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/background/ntp_background_service_factory.h"
#include "chrome/browser/search/background/ntp_custom_background_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"

// static
NtpCustomBackgroundService* NtpCustomBackgroundServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<NtpCustomBackgroundService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
NtpCustomBackgroundServiceFactory*
NtpCustomBackgroundServiceFactory::GetInstance() {
  static base::NoDestructor<NtpCustomBackgroundServiceFactory> instance;
  return instance.get();
}

NtpCustomBackgroundServiceFactory::NtpCustomBackgroundServiceFactory()
    : ProfileKeyedServiceFactory(
          "NtpCustomBackgroundService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(ThemeServiceFactory::GetInstance());
  DependsOn(NtpBackgroundServiceFactory::GetInstance());
}

NtpCustomBackgroundServiceFactory::~NtpCustomBackgroundServiceFactory() =
    default;

std::unique_ptr<KeyedService>
NtpCustomBackgroundServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<NtpCustomBackgroundService>(
      Profile::FromBrowserContext(context));
}

bool NtpCustomBackgroundServiceFactory::ServiceIsCreatedWithBrowserContext()
    const {
  // Any time the theme changes, the NtpCustomBackgroundService must observe the
  // change to react accordingly.
  // To ensure this, we:
  //   (1) register the factory in ChromeBrowserMainExtraPartsProfiles::
  //   EnsureBrowserContextKeyedServiceFactoriesBuilt
  //   (2) Always construct the service alongside the browser context.
  //   (3) Add a dependency on ThemeServiceFactory.
  return true;
}
