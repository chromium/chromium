// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/engagement/site_engagement_service_factory.h"

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/engagement/history_aware_site_engagement_service.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace site_engagement {

// static
SiteEngagementService* SiteEngagementServiceFactory::GetForProfile(
    content::BrowserContext* browser_context) {
  return static_cast<SiteEngagementService*>(
      GetInstance()->GetServiceForBrowserContext(browser_context,
                                                 /*create=*/true));
}

// static
SiteEngagementService* SiteEngagementServiceFactory::GetForProfileIfExists(
    Profile* profile) {
  return static_cast<SiteEngagementService*>(
      GetInstance()->GetServiceForBrowserContext(profile,
                                                 /*create=*/false));
}

// static
SiteEngagementServiceFactory* SiteEngagementServiceFactory::GetInstance() {
  return base::Singleton<SiteEngagementServiceFactory>::get();
}

SiteEngagementServiceFactory::SiteEngagementServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "SiteEngagementService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(HistoryServiceFactory::GetInstance());
  DependsOn(HostContentSettingsMapFactory::GetInstance());
  SiteEngagementService::SetServiceProvider(this);
}

SiteEngagementServiceFactory::~SiteEngagementServiceFactory() {
  SiteEngagementService::ClearServiceProvider(this);
}

KeyedService* SiteEngagementServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  history::HistoryService* history = HistoryServiceFactory::GetForProfile(
      Profile::FromBrowserContext(context), ServiceAccessType::IMPLICIT_ACCESS);
  return new HistoryAwareSiteEngagementService(context, history);
}

content::BrowserContext* SiteEngagementServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}

SiteEngagementService* SiteEngagementServiceFactory::GetSiteEngagementService(
    content::BrowserContext* browser_context) {
  return GetForProfile(browser_context);
}

}  // namespace site_engagement
