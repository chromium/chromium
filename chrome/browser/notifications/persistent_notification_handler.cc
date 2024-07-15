// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/persistent_notification_handler.h"

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "chrome/browser/notifications/metrics/notification_metrics_logger.h"
#include "chrome/browser/notifications/metrics/notification_metrics_logger_factory.h"
#include "chrome/browser/notifications/notification_common.h"
#include "chrome/browser/notifications/notification_permission_context.h"
#include "chrome/browser/notifications/platform_notification_service_factory.h"
#include "chrome/browser/notifications/platform_notification_service_impl.h"
#include "chrome/browser/permissions/notifications_engagement_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_uma_util.h"
#include "components/permissions/permission_util.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_event_dispatcher.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_result.h"
#include "content/public/common/persistent_notification_status.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(ENABLE_BACKGROUND_MODE)
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#endif  // BUILDFLAG(ENABLE_BACKGROUND_MODE)

using content::BrowserThread;

PersistentNotificationHandler::PersistentNotificationHandler() = default;
PersistentNotificationHandler::~PersistentNotificationHandler() = default;

void PersistentNotificationHandler::OnClose(
    Profile* profile,
    const GURL& origin,
    const std::string& notification_id,
    bool by_user,
    base::OnceClosure completed_closure) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(origin.is_valid());

  // TODO(peter): Should we do permission checks prior to forwarding to the
  // NotificationEventDispatcher?

  // If we programmatically closed this notification, don't dispatch any event.
  //
  // TODO(crbug.com/352329050): there are circular dependencies between
  // NotificationMetricsLogger and PlatformNotificationService. Since the
  // service are only created lazily, and creation fails after the shutdown
  // phase, it is possible for the factory to return null. In that case, the
  // notification cannot have been closed programmatically.
  if (PlatformNotificationServiceImpl* service =
          PlatformNotificationServiceFactory::GetForProfile(profile);
      service && service->WasClosedProgrammatically(notification_id)) {
    std::move(completed_closure).Run();
    return;
  }

#if BUILDFLAG(ENABLE_BACKGROUND_MODE)
  close_event_keep_alive_state_.AddKeepAlive(profile);
#endif  // BUILDFLAG(ENABLE_BACKGROUND_MODE)

  NotificationMetricsLogger* metrics_logger =
      NotificationMetricsLoggerFactory::GetForBrowserContext(profile);
  if (by_user)
    metrics_logger->LogPersistentNotificationClosedByUser();
  else
    metrics_logger->LogPersistentNotificationClosedProgrammatically();

  content::NotificationEventDispatcher::GetInstance()
      ->DispatchNotificationCloseEvent(
          profile, notification_id, origin, by_user,
          base::BindOnce(&PersistentNotificationHandler::OnCloseCompleted,
                         weak_ptr_factory_.GetWeakPtr(), profile,
                         std::move(completed_closure)));
}

void PersistentNotificationHandler::OnCloseCompleted(
    Profile* profile,
    base::OnceClosure completed_closure,
    content::PersistentNotificationStatus status) {
  UMA_HISTOGRAM_ENUMERATION(
      "Notifications.PersistentWebNotificationCloseResult", status);

#if BUILDFLAG(ENABLE_BACKGROUND_MODE)
  close_event_keep_alive_state_.RemoveKeepAlive(profile);
#endif  // BUILDFLAG(ENABLE_BACKGROUND_MODE)

  std::move(completed_closure).Run();
}

void PersistentNotificationHandler::OnClick(
    Profile* profile,
    const GURL& origin,
    const std::string& notification_id,
    const std::optional<int>& action_index,
    const std::optional<std::u16string>& reply,
    base::OnceClosure completed_closure) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  NotificationMetricsLogger* metrics_logger =
      NotificationMetricsLoggerFactory::GetForBrowserContext(profile);

#if BUILDFLAG(ENABLE_BACKGROUND_MODE)
  click_event_keep_alive_state_.AddKeepAlive(profile);
#endif  // BUILDFLAG(ENABLE_BACKGROUND_MODE)

  blink::mojom::PermissionStatus permission_status =
      profile->GetPermissionController()
          ->GetPermissionResultForOriginWithoutContext(
              blink::PermissionType::NOTIFICATIONS, url::Origin::Create(origin))
          .status;

  // Don't process click events when the |origin| doesn't have permission. This
  // can't be a DCHECK because of potential races with native notifications.
  if (permission_status != blink::mojom::PermissionStatus::GRANTED) {
    metrics_logger->LogPersistentNotificationClickWithoutPermission();

    OnClickCompleted(profile, notification_id, std::move(completed_closure),
                     content::PersistentNotificationStatus::kPermissionMissing);
    return;
  }

  if (action_index.has_value())
    metrics_logger->LogPersistentNotificationActionButtonClick();
  else
    metrics_logger->LogPersistentNotificationClick();

  // TODO(crbug.com/40280229)
  if (!origin.is_empty()) {
    // Notification clicks are considered a form of engagement with the
    // |origin|, thus we log the interaction with the Site Engagement service.
    site_engagement::SiteEngagementService::Get(profile)
        ->HandleNotificationInteraction(origin);

    auto* service =
        NotificationsEngagementServiceFactory::GetForProfile(profile);
    // This service might be missing for incognito profiles and in tests.
    if (service) {
      service->RecordNotificationInteraction(origin);
    }
  }

  content::NotificationEventDispatcher::GetInstance()
      ->DispatchNotificationClickEvent(
          profile, notification_id, origin, action_index, reply,
          base::BindOnce(&PersistentNotificationHandler::OnClickCompleted,
                         weak_ptr_factory_.GetWeakPtr(), profile,
                         notification_id, std::move(completed_closure)));
}

void PersistentNotificationHandler::OnClickCompleted(
    Profile* profile,
    const std::string& notification_id,
    base::OnceClosure completed_closure,
    content::PersistentNotificationStatus status) {
  UMA_HISTOGRAM_ENUMERATION(
      "Notifications.PersistentWebNotificationClickResult", status);

  switch (status) {
    case content::PersistentNotificationStatus::kSuccess:
    case content::PersistentNotificationStatus::kServiceWorkerError:
    case content::PersistentNotificationStatus::kWaitUntilRejected:
      // There either wasn't a failure, or one that's in the developer's
      // control, so we don't act on the origin's behalf.
      break;
    case content::PersistentNotificationStatus::kServiceWorkerMissing:
    case content::PersistentNotificationStatus::kDatabaseError:
    case content::PersistentNotificationStatus::kPermissionMissing:
      // There was a failure that's out of the developer's control. The user now
      // observes a stuck notification, so let's close it for them.
      PlatformNotificationServiceFactory::GetForProfile(profile)
          ->ClosePersistentNotification(notification_id);
      break;
  }

#if BUILDFLAG(ENABLE_BACKGROUND_MODE)
  click_event_keep_alive_state_.RemoveKeepAlive(profile);
#endif  // BUILDFLAG(ENABLE_BACKGROUND_MODE)

  std::move(completed_closure).Run();
}

void PersistentNotificationHandler::DisableNotifications(Profile* profile,
                                                         const GURL& origin) {
  permissions::PermissionUmaUtil::ScopedRevocationReporter
      scoped_revocation_reporter(
          profile, origin, origin, ContentSettingsType::NOTIFICATIONS,
          permissions::PermissionSourceUI::INLINE_SETTINGS);
#if BUILDFLAG(IS_ANDROID)
  // On Android, NotificationChannelsProviderAndroid does not support moving a
  // channel from ALLOW to BLOCK state, so simply delete the channel instead.
  NotificationPermissionContext::UpdatePermission(profile, origin,
                                                  CONTENT_SETTING_DEFAULT);
#else
  NotificationPermissionContext::UpdatePermission(profile, origin,
                                                  CONTENT_SETTING_BLOCK);
#endif
}

void PersistentNotificationHandler::OpenSettings(Profile* profile,
                                                 const GURL& origin) {
  NotificationCommon::OpenNotificationSettings(profile, origin);
}

#if BUILDFLAG(ENABLE_BACKGROUND_MODE)

PersistentNotificationHandler::NotificationKeepAliveState::
    NotificationKeepAliveState(KeepAliveOrigin keep_alive_origin,
                               ProfileKeepAliveOrigin profile_keep_alive_origin)
    : keep_alive_origin_(keep_alive_origin),
      profile_keep_alive_origin_(profile_keep_alive_origin) {}

PersistentNotificationHandler::NotificationKeepAliveState::
    ~NotificationKeepAliveState() = default;

void PersistentNotificationHandler::NotificationKeepAliveState::AddKeepAlive(
    Profile* profile) {
  // Ensure the browser and Profile stay alive while the event is processed. The
  // keep alives will be reset when all events have been acknowledged.
  if (pending_dispatch_events_++ == 0) {
    event_dispatch_keep_alive_ = std::make_unique<ScopedKeepAlive>(
        keep_alive_origin_, KeepAliveRestartOption::DISABLED);
  }
  // TODO(crbug.com/40159237): Remove IsOffTheRecord() when Incognito profiles
  // support refcounting.
  if (!profile->IsOffTheRecord() &&
      profile_pending_dispatch_events_[profile]++ == 0) {
    event_dispatch_profile_keep_alives_[profile] =
        std::make_unique<ScopedProfileKeepAlive>(profile,
                                                 profile_keep_alive_origin_);
  }
}

void PersistentNotificationHandler::NotificationKeepAliveState::RemoveKeepAlive(
    Profile* profile) {
  DCHECK_GT(pending_dispatch_events_, 0);
  // Reset the keep alive if all in-flight events have been processed.
  if (--pending_dispatch_events_ == 0)
    event_dispatch_keep_alive_.reset();
  // TODO(crbug.com/40159237): Remove IsOffTheRecord() when Incognito profiles
  // support refcounting.
  if (!profile->IsOffTheRecord() &&
      --profile_pending_dispatch_events_[profile] == 0) {
    event_dispatch_profile_keep_alives_[profile].reset();
  }
}

#endif  // BUILDFLAG(ENABLE_BACKGROUND_MODE)
