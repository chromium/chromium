// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ntp_customization/ntp_android_custom_background_service_factory.h"

#include "chrome/browser/ntp_customization/ntp_android_background_service_factory.h"
#include "chrome/browser/ntp_customization/ntp_android_custom_background_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/data_type_store_service_factory.h"
#include "components/prefs/pref_service.h"
#include "components/sync/model/data_type_store_service.h"

// static
NtpAndroidCustomBackgroundService*
NtpAndroidCustomBackgroundServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<NtpAndroidCustomBackgroundService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
NtpAndroidCustomBackgroundServiceFactory*
NtpAndroidCustomBackgroundServiceFactory::GetInstance() {
  static base::NoDestructor<NtpAndroidCustomBackgroundServiceFactory> instance;
  return instance.get();
}

NtpAndroidCustomBackgroundServiceFactory::
    NtpAndroidCustomBackgroundServiceFactory()
    : ProfileKeyedServiceFactory(
          "NtpAndroidCustomBackgroundService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .WithGuest(ProfileSelection::kOriginalOnly)
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(NtpAndroidBackgroundServiceFactory::GetInstance());
  DependsOn(DataTypeStoreServiceFactory::GetInstance());
}

NtpAndroidCustomBackgroundServiceFactory::
    ~NtpAndroidCustomBackgroundServiceFactory() = default;

std::unique_ptr<KeyedService>
NtpAndroidCustomBackgroundServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<NtpAndroidCustomBackgroundService>(
      profile,
      DataTypeStoreServiceFactory::GetForProfile(profile)->GetStoreFactory());
}
