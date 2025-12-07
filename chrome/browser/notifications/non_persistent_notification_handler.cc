// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/non_persistent_notification_handler.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "chrome/browser/notifications/notification_common.h"
#include "chrome/browser/notifications/notification_permission_context.h"
#include "chrome/browser/permissions/notifications_engagement_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/safety_hub/disruptive_notification_permissions_manager.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_uma_util.h"
#include "components/permissions/permission_util.h"
#include "content/public/browser/notification_event_dispatcher.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "ui/message_center/message_center_stats_collector.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/notifications/platform_notification_service_factory.h"
#include "chrome/browser/notifications/platform_notification_service_impl.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "ui/base/page_transition_types.h"
#endif  // !BUILDFLAG(IS_ANDROID)

NonPersistentNotificationHandler::NonPersistentNotificationHandler() = default;
NonPersistentNotificationHandler::~NonPersistentNotificationHandler() = default;

void NonPersistentNotificationHandler::OnShow(
    Profile* profile,
    const std::string& notification_id) {
  content::NotificationEventDispatcher::GetInstance()
      ->DispatchNonPersistentShowEvent(notification_id);
}

void NonPersistentNotificationHandler::OnClose(
    Profile* profile,
    const GURL& origin,
    const std::string& notification_id,
    bool by_user,
    base::OnceClosure completed_closure) {
  content::NotificationEventDispatcher::GetInstance()
      ->DispatchNonPersistentCloseEvent(notification_id,
                                        std::move(completed_closure));
}

void NonPersistentNotificationHandler::OnClick(
    Profile* profile,
    const GURL& origin,
    const std::string& notification_id,
    const std::optional<int>& action_index,
    const std::optional<std::u16string>& reply,
    base::OnceClosure completed_closure) {
  // Non persistent notifications don't allow buttons or replies.
  // https://notifications.spec.whatwg.org/#create-a-notification
  DCHECK(!action_index.has_value());
  DCHECK(!reply.has_value());

  content::NotificationEventDispatcher::GetInstance()
      ->DispatchNonPersistentClickEvent(
          notification_id,
          base::BindOnce(
              &NonPersistentNotificationHandler::DidDispatchClickEvent,
              weak_ptr_factory_.GetWeakPtr(), profile, origin, notification_id,
              std::move(completed_closure)));

  auto* service = NotificationsEngagementServiceFactory::GetForProfile(profile);
  // This service might be missing for incognito profiles and in tests.
  if (service) {
    service->RecordNotificationInteraction(origin);
  }

  // If there is a proposed disruptive notification revocation, report a false
  // positive due to user interacting with a notification. Disruptive are
  // notifications with high notification volume and low site engagement score.
  ukm::SourceId source_id = ukm::UkmRecorder::GetSourceIdForNotificationEvent(
      base::PassKey<NonPersistentNotificationHandler>(), origin);
  DisruptiveNotificationPermissionsManager::MaybeReportFalsePositive(
      profile, origin,
      DisruptiveNotificationPermissionsManager::FalsePositiveReason::
          kNonPersistentNotificationClick,
      source_id);
}

void NonPersistentNotificationHandler::DidDispatchClickEvent(
    Profile* profile,
    const GURL& origin,
    const std::string& notification_id,
    base::OnceClosure completed_closure,
    bool success) {
#if !BUILDFLAG(IS_ANDROID)
  // Non-persistent notifications are able to outlive the document that created
  // them. In such cases the JavaScript event handler might not be available
  // when the notification is interacted with. Launch a new tab for the
  // notification's |origin| instead, and close the activated notification. Not
  // applicable to Android as non-persistent notifications are not available.
  if (!success) {
    NavigateParams params(profile, origin, ui::PAGE_TRANSITION_LINK);

    params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
    params.window_action = NavigateParams::WindowAction::kShowWindow;
    Navigate(&params);

    // Close the |notification_id| as the user has explicitly acknowledged it.
    // Close it asynchronously to prevent use-after-free errors, since the
    // notification itself is actively used by our callers, and it owns the
    // memory of |origin| and |notification_id|.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](Profile* profile, const std::string& notification_id) {
              PlatformNotificationServiceFactory::GetForProfile(profile)
                  ->CloseNotification(notification_id);
            },
            profile, notification_id)
            .Then(std::move(completed_closure)));
  } else {
    std::move(completed_closure).Run();
  }
#endif  // !BUILDFLAG(IS_ANDROID)
}

void NonPersistentNotificationHandler::DisableNotifications(
    Profile* profile,
    const GURL& origin,
    const std::optional<std::string>& notification_id,
    const std::optional<bool>& is_suspicious) {
  permissions::PermissionUmaUtil::ScopedRevocationReporter
      scoped_revocation_reporter(
          profile, origin, origin, ContentSettingsType::NOTIFICATIONS,
          permissions::PermissionSourceUI::INLINE_SETTINGS);
  NotificationPermissionContext::UpdatePermission(profile, origin,
                                                  CONTENT_SETTING_BLOCK);
}

void NonPersistentNotificationHandler::OpenSettings(Profile* profile,
                                                    const GURL& origin) {
  NotificationCommon::OpenNotificationSettings(profile, origin);
  UMA_HISTOGRAM_ENUMERATION(
      "Notifications.Actions",
      message_center::MessageCenterStatsCollector::NotificationActionType::
          NOTIFICATION_ACTION_OPEN_SETTINGS_BUTTON_CLICK,
      message_center::MessageCenterStatsCollector::NotificationActionType::
          NOTIFICATION_ACTION_COUNT);
}
