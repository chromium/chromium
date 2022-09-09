// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/notifications_engagement_service_factory.h"

#include "base/memory/singleton.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/permissions/notifications_engagement_service.h"

// static
NotificationsEngagementServiceFactory*
NotificationsEngagementServiceFactory::GetInstance() {
  return base::Singleton<NotificationsEngagementServiceFactory>::get();
}

// static
permissions::NotificationsEngagementService*
NotificationsEngagementServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<permissions::NotificationsEngagementService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

NotificationsEngagementServiceFactory::NotificationsEngagementServiceFactory()
    : ProfileKeyedServiceFactory("NotificationsEngagementService") {
  DependsOn(HostContentSettingsMapFactory::GetInstance());
}

NotificationsEngagementServiceFactory::
    ~NotificationsEngagementServiceFactory() = default;

KeyedService* NotificationsEngagementServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return new permissions::NotificationsEngagementService(context,
                                                         profile->GetPrefs());
}
