// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feature_guide/notifications/feature_notification_guide_service_factory.h"

#include "base/memory/singleton.h"
#include "base/time/default_clock.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/feature_guide/notifications/feature_notification_guide_service.h"
#include "chrome/browser/feature_guide/notifications/internal/feature_notification_guide_service_impl.h"
#include "chrome/browser/notifications/scheduler/notification_schedule_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"

namespace feature_guide {

// static
FeatureNotificationGuideServiceFactory*
FeatureNotificationGuideServiceFactory::GetInstance() {
  return base::Singleton<FeatureNotificationGuideServiceFactory>::get();
}

// static
FeatureNotificationGuideService*
FeatureNotificationGuideServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<FeatureNotificationGuideService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

FeatureNotificationGuideServiceFactory::FeatureNotificationGuideServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "FeatureNotificationGuideService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(NotificationScheduleServiceFactory::GetInstance());
}

KeyedService* FeatureNotificationGuideServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  auto* notification_scheduler =
      NotificationScheduleServiceFactory::GetForKey(profile->GetProfileKey());
  feature_engagement::Tracker* tracker =
      feature_engagement::TrackerFactory::GetForBrowserContext(profile);
  DCHECK(notification_scheduler);
  DCHECK(tracker);
  // TODO(shaktisahu): Hookup dependencies.
  return new FeatureNotificationGuideServiceImpl();
}

bool FeatureNotificationGuideServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace feature_guide
