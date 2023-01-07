// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accuracy_tips/accuracy_service_factory.h"

#include <memory>

#include "base/feature_list.h"
#include "chrome/browser/accuracy_tips/accuracy_service_delegate.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/engagement/site_engagement_service_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "components/accuracy_tips/accuracy_service.h"
#include "components/accuracy_tips/accuracy_tip_interaction.h"
#include "components/accuracy_tips/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/safe_browsing/core/browser/db/database_manager.h"
#include "components/safe_browsing/core/common/features.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

// static
accuracy_tips::AccuracyService* AccuracyServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<accuracy_tips::AccuracyService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}
// static
AccuracyServiceFactory* AccuracyServiceFactory::GetInstance() {
  return base::Singleton<AccuracyServiceFactory>::get();
}

AccuracyServiceFactory::AccuracyServiceFactory()
    : ProfileKeyedServiceFactory("AccuracyServiceFactory") {
  DependsOn(site_engagement::SiteEngagementServiceFactory::GetInstance());
}

AccuracyServiceFactory::~AccuracyServiceFactory() = default;

// BrowserContextKeyedServiceFactory:
KeyedService* AccuracyServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* browser_context) const {
  DCHECK(base::FeatureList::IsEnabled(safe_browsing::kAccuracyTipsFeature));
  Profile* profile = Profile::FromBrowserContext(browser_context);
  auto sb_database =
      g_browser_process->safe_browsing_service()
          ? g_browser_process->safe_browsing_service()->database_manager()
          : nullptr;
  auto* history_service = HistoryServiceFactory::GetForProfile(
      profile, ServiceAccessType::IMPLICIT_ACCESS);
  auto delegate = std::make_unique<AccuracyServiceDelegate>(profile);
  return new accuracy_tips::AccuracyService(
      std::move(delegate), profile->GetPrefs(), std::move(sb_database),
      history_service, content::GetUIThreadTaskRunner({}),
      content::GetIOThreadTaskRunner({}));
}

void AccuracyServiceFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  if (base::FeatureList::IsEnabled(safe_browsing::kAccuracyTipsFeature))
    accuracy_tips::AccuracyService::RegisterProfilePrefs(registry);
}
