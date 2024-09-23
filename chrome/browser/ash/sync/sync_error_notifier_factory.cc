// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/sync/sync_error_notifier_factory.h"

#include "chrome/browser/ash/sync/sync_error_notifier.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_service_factory.h"

namespace ash {

SyncErrorNotifierFactory::SyncErrorNotifierFactory()
    : ProfileKeyedServiceFactory(
          "SyncErrorNotifier",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(SyncServiceFactory::GetInstance());
}

SyncErrorNotifierFactory::~SyncErrorNotifierFactory() = default;

// static
SyncErrorNotifier* SyncErrorNotifierFactory::GetForProfile(Profile* profile) {
  return static_cast<SyncErrorNotifier*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
SyncErrorNotifierFactory* SyncErrorNotifierFactory::GetInstance() {
  static base::NoDestructor<SyncErrorNotifierFactory> instance;
  return instance.get();
}

std::unique_ptr<KeyedService>
SyncErrorNotifierFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = static_cast<Profile*>(context);
  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile);

  if (!sync_service) {
    return nullptr;
  }

  return std::make_unique<SyncErrorNotifier>(sync_service, profile);
}

}  // namespace ash
