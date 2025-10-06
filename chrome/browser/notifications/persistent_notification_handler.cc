// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/persistent_notification_handler.h"

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/lifetime/termination_notification.h"
#include "chrome/browser/notifications/metrics/notification_metrics_logger.h"
#include "chrome/browser/notifications/metrics/notification_metrics_logger_factory.h"
#include "chrome/browser/notifications/notification_common.h"
#include "chrome/browser/notifications/notification_permission_context.h"
#include "chrome/browser/notifications/platform_notification_service_factory.h"
#include "chrome/browser/notifications/platform_notification_service_impl.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/permissions/notifications_engagement_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/notification_content_detection/notification_content_detection_util.h"
#include "chrome/browser/ui/safety_hub/disruptive_notification_permissions_manager.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_uma_util.h"
#include "components/permissions/permission_util.h"
#include "components/safe_browsing/content/browser/notification_content_detection/notification_content_detection_constants.h"
#include "components/safe_browsing/core/browser/safe_browsing_metrics_collector.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_event_dispatcher.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_descriptor_util.h"
#include "content/public/browser/permission_result.h"
#include "content/public/browser/platform_notification_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/persistent_notification_status.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "ui/message_center/message_center_stats_collector.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(ENABLE_BACKGROUND_MODE)
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#endif  // BUILDFLAG(ENABLE_BACKGROUND_MODE)

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/safe_browsing/android/notification_content_detection_manager_android.h"
#endif  // BUILDFLAG(IS_ANDROID)

using content::BrowserThread;

namespace {

void RecordCloseResult(content::PersistentNotificationStatus status) {
  base::UmaHistogramEnumeration(
      "Notifications.PersistentWebNotificationCloseResult", status);
}

}  // namespace

PersistentNotificationHandler::PersistentNotificationHandler() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  on_app_terminating_subscription_ =
      browser_shutdown::AddAppTerminatingCallback(
          base::BindOnce(&PersistentNotificationHandler::OnAppTerminating,
                         weak_ptr_factory_.GetWeakPtr()));
}

PersistentNotificationHandler::~PersistentNotificationHandler() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void PersistentNotificationHandler::OnClose(
    Profile* profile,
    const GURL& origin,
    const std::string& notification_id,
    bool by_user,
    base::OnceClosure completed_closure) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(origin.is_valid());

  if (browser_shutdown::HasShutdownStarted() ||
      g_browser_process->IsShuttingDown()) {
    // Do not prolong shutdown by running the 'notificationclose' event.
    RecordCloseResult(
        content::PersistentNotificationStatus::kCanceledByAppTerminating);
    std::move(completed_closure).Run();
    return;
  }

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

  int32_t callback_id = close_completed_callbacks_.Add(
      std::make_unique<base::OnceClosure>(std::move(completed_closure)));

  content::NotificationEventDispatcher::GetInstance()
      ->DispatchNotificationCloseEvent(
          profile, notification_id, origin, by_user,
          base::BindOnce(&PersistentNotificationHandler::OnCloseCompleted,
                         weak_ptr_factory_.GetWeakPtr(), profile, callback_id));
}

void PersistentNotificationHandler::OnCloseCompleted(
    Profile* profile,
    uint64_t close_completed_callback_id,
    content::PersistentNotificationStatus status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  base::OnceClosure* completed_closure_pointer =
      close_completed_callbacks_.Lookup(close_completed_callback_id);
  if (!completed_closure_pointer) {
    // `OnAppTerminating()` already ran the callback.
    return;
  }

  base::OnceClosure completed_closure = std::move(*completed_closure_pointer);
  close_completed_callbacks_.Remove(close_completed_callback_id);

  RecordCloseResult(status);

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
              content::PermissionDescriptorUtil::
                  CreatePermissionDescriptorForPermissionType(
                      blink::PermissionType::NOTIFICATIONS),
              url::Origin::Create(origin))
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
    auto* service =
        NotificationsEngagementServiceFactory::GetForProfile(profile);
    // This service might be missing for incognito profiles and in tests.
    if (service) {
      service->RecordNotificationInteraction(origin);
    }

    // Notification clicks are considered a form of engagement with the
    // |origin|, thus we log the interaction with the Site Engagement service.
    site_engagement::SiteEngagementService::Get(profile)
        ->HandleNotificationInteraction(origin);
  }

  content::NotificationEventDispatcher::GetInstance()
      ->DispatchNotificationClickEvent(
          profile, notification_id, origin, action_index, reply,
          base::BindOnce(&PersistentNotificationHandler::OnClickCompleted,
                         weak_ptr_factory_.GetWeakPtr(), profile,
                         notification_id, std::move(completed_closure)));

  // If there is a proposed disruptive notification revocation, report a false
  // positive due to user interacting with a notification. Disruptive are
  // notifications with high notification volume and low site engagement score.
  ukm::SourceId source_id = ukm::UkmRecorder::GetSourceIdForNotificationEvent(
      base::PassKey<PersistentNotificationHandler>(), origin);
  DisruptiveNotificationPermissionsManager::MaybeReportFalsePositive(
      profile, origin,
      DisruptiveNotificationPermissionsManager::FalsePositiveReason::
          kPersistentNotificationClick,
      source_id);
}

void PersistentNotificationHandler::OnClickCompleted(
    Profile* profile,
    const std::string& notification_id,
    base::OnceClosure completed_closure,
    content::PersistentNotificationStatus status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

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

    case content::PersistentNotificationStatus::kCanceledByAppTerminating:
      // App termination must not cancel the click event.
      NOTREACHED();
  }

#if BUILDFLAG(ENABLE_BACKGROUND_MODE)
  click_event_keep_alive_state_.RemoveKeepAlive(profile);
#endif  // BUILDFLAG(ENABLE_BACKGROUND_MODE)

  std::move(completed_closure).Run();
}

void PersistentNotificationHandler::OnAppTerminating() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Release all keep alives for currently running 'notificationclose' events.
  // This will allow browser shutdown to begin without waiting for the
  // 'notificationclose' events to complete.
#if BUILDFLAG(ENABLE_BACKGROUND_MODE)
  close_event_keep_alive_state_.RemoveAllKeepAlives();
#endif

  // Like `OnCloseCompleted()` above, run the 'notificationclose' completed
  // callbacks after removing the keep alives.
  for (CloseCompletedCallbackMap::iterator it(&close_completed_callbacks_);
       !it.IsAtEnd(); it.Advance()) {
    RecordCloseResult(
        content::PersistentNotificationStatus::kCanceledByAppTerminating);
    std::move(*it.GetCurrentValue()).Run();
  }
  close_completed_callbacks_.Clear();
}

void PersistentNotificationHandler::DisableNotifications(
    Profile* profile,
    const GURL& origin,
    const std::optional<std::string>& notification_id,
    const std::optional<bool>& is_suspicious) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
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
  // Remove `origin` from user allowlisted sites when user unsubscribes. On
  // Android, log the suspicious notification unsubscribe ukm event if the
  // notification was suspicious.
  auto* hcsm = HostContentSettingsMapFactory::GetForProfile(profile);
  if (hcsm && origin.is_valid()) {
    hcsm->SetWebsiteSettingCustomScope(
        ContentSettingsPattern::FromURLNoWildcard(origin),
        ContentSettingsPattern::Wildcard(),
        ContentSettingsType::ARE_SUSPICIOUS_NOTIFICATIONS_ALLOWLISTED_BY_USER,
        base::Value(base::Value::Dict().Set(
            safe_browsing::kIsAllowlistedByUserKey, false)));
#if BUILDFLAG(IS_ANDROID)
    if (notification_id.has_value()) {
      safe_browsing::MaybeLogSuspiciousNotificationUnsubscribeUkm(
          hcsm, origin, notification_id.value(), profile);
    }
    if (is_suspicious.has_value()) {
      safe_browsing::SafeBrowsingMetricsCollector::
          LogSafeBrowsingNotificationRevocationSourceHistogram(
              is_suspicious.value()
                  ? safe_browsing::NotificationRevocationSource::
                        kSuspiciousWarningOneTapUnsubscribe
                  : safe_browsing::NotificationRevocationSource::
                        kStandardOneTapUnsubscribe);
    }
#endif
  }
}

void PersistentNotificationHandler::OpenSettings(Profile* profile,
                                                 const GURL& origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  NotificationCommon::OpenNotificationSettings(profile, origin);
  UMA_HISTOGRAM_ENUMERATION(
      "Notifications.Actions",
      message_center::MessageCenterStatsCollector::NotificationActionType::
          NOTIFICATION_ACTION_OPEN_SETTINGS_BUTTON_CLICK,
      message_center::MessageCenterStatsCollector::NotificationActionType::
          NOTIFICATION_ACTION_COUNT);
}

void PersistentNotificationHandler::ReportNotificationAsSafe(
    const std::string& notification_id,
    const GURL& url,
    Profile* profile) {
  OnMaybeReport(notification_id, url, profile, /*did_show_warning=*/true,
                /*did_user_unsubscribe=*/false);
}

void PersistentNotificationHandler::ReportWarnedNotificationAsSpam(
    const std::string& notification_id,
    const GURL& url,
    Profile* profile) {
  OnMaybeReport(notification_id, url, profile, /*did_show_warning=*/true,
                /*did_user_unsubscribe=*/true);
}

void PersistentNotificationHandler::ReportUnwarnedNotificationAsSpam(
    const std::string& notification_id,
    const GURL& url,
    Profile* profile) {
  OnMaybeReport(notification_id, url, profile, /*did_show_warning=*/false,
                /*did_user_unsubscribe=*/true);
}

void PersistentNotificationHandler::OnShowOriginalNotification(
    const GURL& url,
    const std::string& notification_id,
    Profile* profile) {
#if BUILDFLAG(IS_ANDROID)
  safe_browsing::NotificationContentDetectionUkmUtil::
      RecordSuspiciousNotificationInteractionUkm(
          static_cast<int>(
              safe_browsing::SuspiciousNotificationWarningInteractions::
                  kShowOriginalNotification),
          url, notification_id, profile);
  if (base::FeatureList::IsEnabled(
          safe_browsing::kAutoRevokeSuspiciousNotification)) {
    auto* hcsm = HostContentSettingsMapFactory::GetForProfile(profile);
    if (hcsm && !url.is_empty()) {
      hcsm->SetWebsiteSettingCustomScope(
          ContentSettingsPattern::FromURLNoWildcard(url),
          ContentSettingsPattern::Wildcard(),
          ContentSettingsType::SUSPICIOUS_NOTIFICATION_SHOW_ORIGINAL,
          base::Value(base::Value::Dict().Set(
              safe_browsing::kSuspiciousNotificationShowOriginalKey, true)));
    }
  }
#endif
}

void PersistentNotificationHandler::OnMaybeReport(
    const std::string& notification_id,
    const GURL& url,
    Profile* profile,
    bool did_show_warning,
    bool did_user_unsubscribe) {
  CHECK(profile);

  // In case the data volume becomes excessive, logging should happen at a
  // sampled rate. This rate is defined by the
  // `kReportNotificationContentDetectionDataRate` feature parameter.
  if (base::RandDouble() * 100 >
      safe_browsing::kReportNotificationContentDetectionDataRate.Get()) {
    return;
  }

  scoped_refptr<content::PlatformNotificationContext> notification_context =
      profile->GetStoragePartitionForUrl(url)->GetPlatformNotificationContext();
  if (!notification_context ||
      !OptimizationGuideKeyedServiceFactory::GetForProfile(profile)) {
    return;
  }

  blink::mojom::EngagementLevel engagement_level =
      blink::mojom::EngagementLevel::NONE;
  if (site_engagement::SiteEngagementService::Get(profile)) {
    engagement_level = site_engagement::SiteEngagementService::Get(profile)
                           ->GetEngagementLevel(url);
  }

  // Read notification data from database and upload as log to model quality
  // service.
  notification_context->ReadNotificationDataAndRecordInteraction(
      notification_id, url,
      content::PlatformNotificationContext::Interaction::NONE,
      base::BindOnce(
          &safe_browsing::SendNotificationContentDetectionDataToMQLSServer,
          OptimizationGuideKeyedServiceFactory::GetForProfile(profile)
              ->GetModelQualityLogsUploaderService()
              ->GetWeakPtr(),
          safe_browsing::NotificationContentDetectionMQLSMetadata(
              did_show_warning, did_user_unsubscribe, engagement_level)));
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

void PersistentNotificationHandler::NotificationKeepAliveState::
    RemoveAllKeepAlives() {
  event_dispatch_keep_alive_.reset();
  event_dispatch_profile_keep_alives_.clear();
  pending_dispatch_events_ = 0;
  profile_pending_dispatch_events_.clear();
}

#endif  // BUILDFLAG(ENABLE_BACKGROUND_MODE)
