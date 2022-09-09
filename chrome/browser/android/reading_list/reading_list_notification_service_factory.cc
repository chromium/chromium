// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/reading_list/reading_list_notification_service_factory.h"

#include <memory>

#include "base/time/default_clock.h"
#include "chrome/browser/android/reading_list/reading_list_bridge.h"
#include "chrome/browser/notifications/scheduler/notification_schedule_service_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/reading_list/android/reading_list_notification_service.h"
#include "chrome/browser/ui/read_later/reading_list_model_factory.h"
#include "components/reading_list/features/reading_list_switches.h"

// static
ReadingListNotificationServiceFactory*
ReadingListNotificationServiceFactory::GetInstance() {
  return base::Singleton<ReadingListNotificationServiceFactory>::get();
}

// static
ReadingListNotificationService*
ReadingListNotificationServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<ReadingListNotificationService*>(
      GetInstance()->GetServiceForBrowserContext(context, /*create=*/true));
}

ReadingListNotificationServiceFactory::ReadingListNotificationServiceFactory()
    : ProfileKeyedServiceFactory(
          "ReadingListNotificationService",
          ProfileSelections::BuildRedirectedInIncognito()) {
  DependsOn(ReadingListModelFactory::GetInstance());
  DependsOn(NotificationScheduleServiceFactory::GetInstance());
}

ReadingListNotificationServiceFactory::
    ~ReadingListNotificationServiceFactory() = default;

KeyedService* ReadingListNotificationServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  auto* reading_list_model =
      ReadingListModelFactory::GetForBrowserContext(context);
  Profile* profile = Profile::FromBrowserContext(context);
  auto* notification_scheduler =
      NotificationScheduleServiceFactory::GetForKey(profile->GetProfileKey());
  auto config = std::make_unique<ReadingListNotificationService::Config>();
  config->notification_show_time = 8;
  return new ReadingListNotificationServiceImpl(
      reading_list_model, notification_scheduler,
      std::make_unique<ReadingListBridge>(), std::move(config),
      base::DefaultClock::GetInstance());
}
