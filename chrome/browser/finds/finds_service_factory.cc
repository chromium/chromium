// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/finds/finds_service_factory.h"

#include "base/feature_list.h"
#include "chrome/browser/finds/core/finds_features.h"
#include "chrome/browser/finds/core/finds_service.h"
#include "chrome/browser/finds/core/finds_utils.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/notifications/scheduler/notification_schedule_service_factory.h"
#include "chrome/browser/notifications/scheduler/public/notification_schedule_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/prefs/pref_service.h"
#include "components/sync/service/sync_service.h"

namespace finds {

// static
FindsServiceFactory* FindsServiceFactory::GetInstance() {
  static base::NoDestructor<FindsServiceFactory> instance;
  return instance.get();
}

// static
FindsService* FindsServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<FindsService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

FindsServiceFactory::FindsServiceFactory()
    : ProfileKeyedServiceFactory(
          "FindsService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .WithGuest(ProfileSelection::kOriginalOnly)
              .WithAshInternals(ProfileSelection::kNone)
              .Build()) {
  DependsOn(OptimizationGuideKeyedServiceFactory::GetInstance());
  DependsOn(HistoryServiceFactory::GetInstance());
  DependsOn(NotificationScheduleServiceFactory::GetInstance());
  DependsOn(SyncServiceFactory::GetInstance());
}

FindsServiceFactory::~FindsServiceFactory() = default;

std::unique_ptr<KeyedService>
FindsServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!base::FeatureList::IsEnabled(finds::features::kChromeFinds)) {
    return nullptr;
  }
  Profile* profile = Profile::FromBrowserContext(context);

  if (!IsAllowedByEnterprisePolicy(profile->GetPrefs())) {
    return nullptr;
  }

  OptimizationGuideKeyedService* opt_guide_service =
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile);
  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(profile,
                                           ServiceAccessType::EXPLICIT_ACCESS);
  notifications::NotificationScheduleService* notification_schedule_service =
      NotificationScheduleServiceFactory::GetForKey(profile->GetProfileKey());
  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile);

  return std::make_unique<FindsService>(
      opt_guide_service, history_service, profile->GetPrefs(),
      notification_schedule_service, sync_service);
}

}  // namespace finds
