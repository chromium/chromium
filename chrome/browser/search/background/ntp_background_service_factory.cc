// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/background/ntp_background_service_factory.h"

#include <optional>
#include <string>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/background/ntp_background_service.h"
#include "components/search/ntp_features.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"

// static
NtpBackgroundService* NtpBackgroundServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<NtpBackgroundService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
NtpBackgroundServiceFactory* NtpBackgroundServiceFactory::GetInstance() {
  static base::NoDestructor<NtpBackgroundServiceFactory> instance;
  return instance.get();
}

NtpBackgroundServiceFactory::NtpBackgroundServiceFactory()
    : ProfileKeyedServiceFactory(
          "NtpBackgroundService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {}

NtpBackgroundServiceFactory::~NtpBackgroundServiceFactory() = default;

std::unique_ptr<KeyedService>
NtpBackgroundServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  // TODO(crbug.com/41431683): Background service URLs should be
  // configurable server-side, so they can be changed mid-release.

  return std::make_unique<NtpBackgroundService>(
      Profile::FromBrowserContext(context)->GetURLLoaderFactory());
}
