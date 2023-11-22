// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_trigger_scheduler.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/platform_notification_service_factory.h"
#include "chrome/browser/notifications/platform_notification_service_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/platform_notification_context.h"
#include "content/public/browser/storage_partition.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/notifications/notification_trigger_scheduler_android.h"
#endif

using content::BrowserContext;
using content::BrowserThread;

// static
std::unique_ptr<NotificationTriggerScheduler>
NotificationTriggerScheduler::Create() {
#if BUILDFLAG(IS_ANDROID)
  return base::WrapUnique(new NotificationTriggerSchedulerAndroid());
#else
  return base::WrapUnique(new NotificationTriggerScheduler());
#endif
}

// static
void NotificationTriggerScheduler::TriggerNotifications() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Skip if the browser process is already in shutdown path.
  if (!g_browser_process || g_browser_process->IsShuttingDown())
    return;
  auto profiles = g_browser_process->profile_manager()->GetLoadedProfiles();
  for (Profile* profile : profiles) {
    TriggerNotificationsForProfile(profile);
    // Notifications are technically not supported in OffTheRecord, but in case
    // we ever change that lets handle these profiles too.
    if (profile->HasAnyOffTheRecordProfile()) {
      std::vector<Profile*> otr_profiles =
          profile->GetAllOffTheRecordProfiles();
      for (Profile* otr : otr_profiles)
        TriggerNotificationsForProfile(otr);
    }
  }
}

NotificationTriggerScheduler::NotificationTriggerScheduler() = default;

NotificationTriggerScheduler::~NotificationTriggerScheduler() = default;

void NotificationTriggerScheduler::TriggerNotificationsForStoragePartition(
    content::StoragePartition* partition) {
  partition->GetPlatformNotificationContext()->TriggerNotifications();
}

void NotificationTriggerScheduler::TriggerNotificationsForProfile(
    Profile* profile) {
  auto* service = PlatformNotificationServiceFactory::GetForProfile(profile);
  // Service might not be available for some irregular profiles, like the System
  // Profile.
  if (!service)
    return;

  base::Time next_trigger = service->ReadNextTriggerTimestamp();

  // Skip this profile if there are no pending notifications.
  if (next_trigger > base::Time::Now()) {
    return;
  }

  // Reset the next trigger time. It will be set again if there are more
  // scheduled notifications for any storage partition of this profile.
  profile->GetPrefs()->SetTime(prefs::kNotificationNextTriggerTime,
                               base::Time::Max());

  NotificationTriggerScheduler* const scheduler =
      service->GetNotificationTriggerScheduler();
  profile->ForEachLoadedStoragePartition(
      [&](content::StoragePartition* partition) {
        scheduler->TriggerNotificationsForStoragePartition(partition);
      });
}
