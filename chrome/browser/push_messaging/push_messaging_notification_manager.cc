// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/push_messaging/push_messaging_notification_manager.h"

#include <stddef.h>

#include <bitset>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/notifications/platform_notification_service_factory.h"
#include "chrome/browser/notifications/platform_notification_service_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/push_messaging/push_messaging_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/platform_notification_context.h"
#include "content/public/browser/push_messaging_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/page_visibility_state.h"
#include "content/public/common/url_constants.h"
#include "extensions/buildflags/buildflags.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "third_party/blink/public/common/notifications/notification_resources.h"
#include "third_party/blink/public/mojom/notifications/notification.mojom-shared.h"
#include "third_party/blink/public/mojom/push_messaging/push_messaging_status.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/manifest_handlers/background_info.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#else
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#endif

using content::BrowserThread;
using content::NotificationDatabaseData;
using content::PlatformNotificationContext;
using content::PushMessagingService;
using content::ServiceWorkerContext;
using content::WebContents;

namespace {

content::StoragePartition* GetStoragePartition(Profile* profile,
                                               const GURL& origin) {
  return profile->GetStoragePartitionForUrl(origin);
}

NotificationDatabaseData CreateDatabaseData(
    const GURL& origin,
    int64_t service_worker_registration_id) {
  blink::PlatformNotificationData notification_data;
  notification_data.title = url_formatter::FormatUrlForSecurityDisplay(
      origin, url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS);
  notification_data.direction =
      blink::mojom::NotificationDirection::LEFT_TO_RIGHT;
  notification_data.body =
      l10n_util::GetStringUTF16(IDS_PUSH_MESSAGING_GENERIC_NOTIFICATION_BODY);
  notification_data.tag = kPushMessagingForcedNotificationTag;
  notification_data.icon = GURL();
  notification_data.timestamp = base::Time::Now();
  notification_data.silent = true;

  NotificationDatabaseData database_data;
  database_data.origin = origin;
  database_data.service_worker_registration_id = service_worker_registration_id;
  database_data.notification_data = notification_data;

  // Make sure we don't expose this notification to the site.
  database_data.is_shown_by_browser = true;

  return database_data;
}

}  // namespace

PushMessagingNotificationManager::PushMessagingNotificationManager(
    Profile* profile)
    : profile_(profile), budget_database_(profile) {}

PushMessagingNotificationManager::~PushMessagingNotificationManager() = default;

void PushMessagingNotificationManager::EnforceUserVisibleOnlyRequirements(
    const GURL& origin,
    int64_t service_worker_registration_id,
    EnforceRequirementsCallback message_handled_callback,
    bool requested_user_visible_only) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (ShouldSkipUserVisibleOnlyRequirements(origin,
                                            requested_user_visible_only)) {
    std::move(message_handled_callback)
        .Run(/* did_show_generic_notification= */ false);
    LogSilentPushEvent(SilentPushEvent::kNotificationEnforcementSkipped);
    return;
  }

  // TODO(johnme): Relax this heuristic slightly.
  scoped_refptr<PlatformNotificationContext> notification_context =
      GetStoragePartition(profile_, origin)->GetPlatformNotificationContext();

  notification_context->CountVisibleNotificationsForServiceWorkerRegistration(
      origin, service_worker_registration_id,
      base::BindOnce(
          &PushMessagingNotificationManager::DidCountVisibleNotifications,
          weak_factory_.GetWeakPtr(), origin, service_worker_registration_id,
          std::move(message_handled_callback)));
}

void PushMessagingNotificationManager::DidCountVisibleNotifications(
    const GURL& origin,
    int64_t service_worker_registration_id,
    EnforceRequirementsCallback message_handled_callback,
    bool success,
    int notification_count) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // TODO(johnme): Hiding an existing notification should also count as a useful
  // user-visible action done in response to a push message - but make sure that
  // sending two messages in rapid succession which show then hide a
  // notification doesn't count.
  // TODO(crbug.com/40596304): Scheduling a notification should count as a
  // user-visible action, if it is not immediately cancelled or the |origin|
  // schedules too many notifications too far in the future.
  bool notification_shown = notification_count > 0;
  bool notification_needed = true;

  // Sites with a currently visible tab don't need to show notifications.
#if BUILDFLAG(IS_ANDROID)
  for (const TabModel* model : TabModelList::models()) {
    Profile* profile = model->GetProfile();
    WebContents* active_web_contents = model->GetActiveWebContents();
#else
  for (Browser* browser : *BrowserList::GetInstance()) {
    Profile* profile = browser->profile();
    WebContents* active_web_contents =
        browser->tab_strip_model()->GetActiveWebContents();
#endif
    if (IsTabVisible(profile, active_web_contents, origin)) {
      notification_needed = false;
      break;
    }
  }

  // If more than one notification is showing for this Service Worker, close
  // the default notification if it happens to be part of this group.
  if (notification_count >= 2) {
    scoped_refptr<PlatformNotificationContext> notification_context =
        GetStoragePartition(profile_, origin)->GetPlatformNotificationContext();
    notification_context->DeleteAllNotificationDataWithTag(
        kPushMessagingForcedNotificationTag, /*is_shown_by_browser=*/true,
        origin, base::DoNothing());
  }

  if (notification_needed && !notification_shown) {
    // If the worker needed to show a notification and didn't, see if a silent
    // push was allowed.
    budget_database_.SpendBudget(
        url::Origin::Create(origin),
        base::BindOnce(&PushMessagingNotificationManager::ProcessSilentPush,
                       weak_factory_.GetWeakPtr(), origin,
                       service_worker_registration_id,
                       std::move(message_handled_callback)));
    return;
  }

  std::move(message_handled_callback)
      .Run(/* did_show_generic_notification= */ false);
}

bool PushMessagingNotificationManager::IsTabVisible(
    Profile* profile,
    WebContents* active_web_contents,
    const GURL& origin) {
  if (!active_web_contents || !active_web_contents->GetPrimaryMainFrame())
    return false;

  // Don't leak information from other profiles.
  if (profile != profile_)
    return false;

  // Ignore minimized windows.
  switch (active_web_contents->GetPrimaryMainFrame()->GetVisibilityState()) {
    case content::PageVisibilityState::kHidden:
    case content::PageVisibilityState::kHiddenButPainting:
      return false;
    case content::PageVisibilityState::kVisible:
      break;
  }

  // Use the visible URL since that's the one the user is aware of (and it
  // doesn't matter whether the page loaded successfully).
  GURL visible_url = active_web_contents->GetVisibleURL();

  // view-source: pages are considered to be controlled Service Worker clients
  // and thus should be considered when checking the visible URL. However, the
  // prefix has to be removed before the origins can be compared.
  if (visible_url.SchemeIs(content::kViewSourceScheme))
    visible_url = GURL(visible_url.GetContent());

  return visible_url.DeprecatedGetOriginAsURL() == origin;
}

void PushMessagingNotificationManager::ProcessSilentPush(
    const GURL& origin,
    int64_t service_worker_registration_id,
    EnforceRequirementsCallback message_handled_callback,
    bool silent_push_allowed) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  LogSilentPushEvent(SilentPushEvent::kSilentRequest);

  // If the origin was allowed to issue a silent push, just return.
  if (silent_push_allowed) {
    std::move(message_handled_callback)
        .Run(/* did_show_generic_notification= */ false);
    LogSilentPushEvent(SilentPushEvent::kAllowedWithoutNotification);
    return;
  }

  // The site failed to show a notification when one was needed, and they don't
  // have enough budget to cover the cost of suppressing, so we will show a
  // generic notification.
  NotificationDatabaseData database_data =
      CreateDatabaseData(origin, service_worker_registration_id);
  scoped_refptr<PlatformNotificationContext> notification_context =
      GetStoragePartition(profile_, origin)->GetPlatformNotificationContext();
  int64_t next_persistent_notification_id =
      PlatformNotificationServiceFactory::GetForProfile(profile_)
          ->ReadNextPersistentNotificationId();

  notification_context->WriteNotificationData(
      next_persistent_notification_id, service_worker_registration_id, origin,
      database_data,
      base::BindOnce(
          &PushMessagingNotificationManager::DidWriteNotificationData,
          weak_factory_.GetWeakPtr(), std::move(message_handled_callback)));
}

void PushMessagingNotificationManager::DidWriteNotificationData(
    EnforceRequirementsCallback message_handled_callback,
    bool success,
    const std::string& notification_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!success)
    DLOG(ERROR) << "Writing forced notification to database should not fail";

  std::move(message_handled_callback)
      .Run(/* did_show_generic_notification= */ true);
  LogSilentPushEvent(SilentPushEvent::kAllowedWithGenericNotification);
}

bool PushMessagingNotificationManager::ShouldSkipUserVisibleOnlyRequirements(
    const GURL& origin,
    bool requested_user_visible_only) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (origin.SchemeIs(extensions::kExtensionScheme)) {
    return ShouldSkipExtensionUserVisibleOnlyRequirements(
        origin, requested_user_visible_only);
  }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

  // Returning true is an exception, so default to deny for anything we don't
  // explicitly identify.
  return false;
}

void PushMessagingNotificationManager::LogSilentPushEvent(
    SilentPushEvent event) {
  UMA_HISTOGRAM_ENUMERATION("PushMessaging.SilentNotification", event);
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
bool PushMessagingNotificationManager::
    ShouldSkipExtensionUserVisibleOnlyRequirements(
        const GURL& origin,
        bool requested_user_visible_only) {
  // Worker based extensions are exempt from the user visible requirement only
  // if they request it.
  if (!requested_user_visible_only) {
    return false;
  }
  const extensions::ExtensionSet& extensions =
      extensions::ExtensionRegistry::Get(profile_)->enabled_extensions();
  const extensions::Extension* extension =
      extensions.GetExtensionOrAppByURL(origin);
  if (!extension) {
    return false;
  }
  return extensions::BackgroundInfo::IsServiceWorkerBased(extension);
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
