// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/background/ntp_custom_background_service_factory.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/background/ntp_custom_background_service.h"
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
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              .Build()) {}

NtpCustomBackgroundServiceFactory::~NtpCustomBackgroundServiceFactory() =
    default;

std::unique_ptr<KeyedService>
NtpCustomBackgroundServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<NtpCustomBackgroundService>(
      Profile::FromBrowserContext(context));
}
