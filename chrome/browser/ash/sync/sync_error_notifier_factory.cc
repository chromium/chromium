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
    : ProfileKeyedServiceFactory("SyncErrorNotifier") {
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
  return base::Singleton<SyncErrorNotifierFactory>::get();
}

KeyedService* SyncErrorNotifierFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = static_cast<Profile*>(context);
  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile);

  if (!sync_service) {
    return nullptr;
  }

  return new SyncErrorNotifier(sync_service, profile);
}

}  // namespace ash
