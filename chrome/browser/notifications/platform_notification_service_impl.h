// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_PLATFORM_NOTIFICATION_SERVICE_IMPL_H_
#define CHROME_BROWSER_NOTIFICATIONS_PLATFORM_NOTIFICATION_SERVICE_IMPL_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <unordered_set>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/task/cancelable_task_tracker.h"
#include "chrome/browser/notifications/notification_common.h"
#include "chrome/browser/notifications/notification_trigger_scheduler.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/buildflags.h"
#include "components/content_settings/core/browser/content_settings_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/platform_notification_service.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "ui/message_center/public/cpp/notification.h"

class GURL;
class Profile;

namespace content {
struct NotificationResources;
}  // namespace content

// The platform notification service is the profile-specific entry point through
// which Web Notifications can be controlled.
class PlatformNotificationServiceImpl
    : public content::PlatformNotificationService,
      public content_settings::Observer,
      public KeyedService {
 public:
  explicit PlatformNotificationServiceImpl(Profile* profile);
  ~PlatformNotificationServiceImpl() override;

  // Register profile-specific prefs.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Returns whether the notification identified by |notification_id| was
  // closed programmatically through ClosePersistentNotification().
  bool WasClosedProgrammatically(const std::string& notification_id);

  // content::PlatformNotificationService implementation.
  void DisplayNotification(
      const std::string& notification_id,
      const GURL& origin,
      const blink::PlatformNotificationData& notification_data,
      const blink::NotificationResources& notification_resources) override;
  void DisplayPersistentNotification(
      const std::string& notification_id,
      const GURL& service_worker_scope,
      const GURL& origin,
      const blink::PlatformNotificationData& notification_data,
      const blink::NotificationResources& notification_resources) override;
  void CloseNotification(const std::string& notification_id) override;
  void ClosePersistentNotification(const std::string& notification_id) override;
  void GetDisplayedNotifications(
      DisplayedNotificationsCallback callback) override;
  void ScheduleTrigger(base::Time timestamp) override;
  base::Time ReadNextTriggerTimestamp() override;
  int64_t ReadNextPersistentNotificationId() override;
  void RecordNotificationUkmEvent(
      const content::NotificationDatabaseData& data) override;

  void set_ukm_recorded_closure_for_testing(base::OnceClosure closure) {
    ukm_recorded_closure_for_testing_ = std::move(closure);
  }

  NotificationTriggerScheduler* GetNotificationTriggerScheduler();

 private:
  friend class NotificationTriggerSchedulerTest;
  friend class PersistentNotificationHandlerTest;
  friend class PlatformNotificationServiceBrowserTest;
  friend class PlatformNotificationServiceTest;
  friend class PushMessagingBrowserTest;
  FRIEND_TEST_ALL_PREFIXES(PlatformNotificationServiceTest,
                           CreateNotificationFromData);
  FRIEND_TEST_ALL_PREFIXES(PlatformNotificationServiceTest,
                           DisplayNameForContextMessage);
  FRIEND_TEST_ALL_PREFIXES(PlatformNotificationServiceTest,
                           RecordNotificationUkmEvent);

  // KeyedService implementation.
  void Shutdown() override;

  // content_settings::Observer implementation.
  void OnContentSettingChanged(const ContentSettingsPattern& primary_pattern,
                               const ContentSettingsPattern& secondary_pattern,
                               ContentSettingsType content_type,
                               const std::string& resource_identifier) override;

  static void DidGetBackgroundSourceId(
      base::OnceClosure recorded_closure,
      const content::NotificationDatabaseData& data,
      base::Optional<ukm::SourceId> source_id);

  // Creates a new Web Notification-based Notification object. Should only be
  // called when the notification is first shown.
  message_center::Notification CreateNotificationFromData(
      const GURL& origin,
      const std::string& notification_id,
      const blink::PlatformNotificationData& notification_data,
      const blink::NotificationResources& notification_resources) const;

  // Returns a display name for an origin, to be used in the context message
  base::string16 DisplayNameForContextMessage(const GURL& origin) const;

  // Clears |closed_notifications_|. Should only be used for testing purposes.
  void ClearClosedNotificationsForTesting() { closed_notifications_.clear(); }

  // The profile for this instance or NULL if the initial profile has been
  // shutdown already.
  Profile* profile_;

  // Tracks the id of persistent notifications that have been closed
  // programmatically to avoid dispatching close events for them.
  std::unordered_set<std::string> closed_notifications_;

  // Scheduler for notifications with a trigger.
  std::unique_ptr<NotificationTriggerScheduler> trigger_scheduler_;

  // Testing-only closure to observe when a UKM event has been recorded.
  base::OnceClosure ukm_recorded_closure_for_testing_;

  DISALLOW_COPY_AND_ASSIGN(PlatformNotificationServiceImpl);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_PLATFORM_NOTIFICATION_SERVICE_IMPL_H_
