// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/push_messaging/push_messaging_notification_manager.h"

#include <stddef.h>

#include <bitset>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/notifications/platform_notification_service_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/push_messaging/push_messaging_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/rappor/public/rappor_utils.h"
#include "components/rappor/rappor_service_impl.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/platform_notification_context.h"
#include "content/public/browser/push_messaging_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/push_messaging_status.mojom.h"
#include "content/public/common/url_constants.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "third_party/blink/public/common/notifications/notification_resources.h"
#include "third_party/blink/public/mojom/page/page_visibility_state.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

#if defined(OS_ANDROID)
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
void RecordUserVisibleStatus(content::mojom::PushUserVisibleStatus status) {
  UMA_HISTOGRAM_ENUMERATION("PushMessaging.UserVisibleStatus", status);
}

content::StoragePartition* GetStoragePartition(Profile* profile,
                                               const GURL& origin) {
  return content::BrowserContext::GetStoragePartitionForSite(profile, origin);
}

NotificationDatabaseData CreateDatabaseData(
    const GURL& origin,
    int64_t service_worker_registration_id) {
  blink::PlatformNotificationData notification_data;
  notification_data.title = url_formatter::FormatUrlForSecurityDisplay(
      origin, url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS);
  notification_data.direction =
      blink::PlatformNotificationData::DIRECTION_LEFT_TO_RIGHT;
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
  return database_data;
}

}  // namespace

PushMessagingNotificationManager::PushMessagingNotificationManager(
    Profile* profile)
    : profile_(profile), budget_database_(profile), weak_factory_(this) {}

PushMessagingNotificationManager::~PushMessagingNotificationManager() {}

void PushMessagingNotificationManager::EnforceUserVisibleOnlyRequirements(
    const GURL& origin,
    int64_t service_worker_registration_id,
    const base::Closure& message_handled_closure) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // TODO(johnme): Relax this heuristic slightly.
  scoped_refptr<PlatformNotificationContext> notification_context =
      GetStoragePartition(profile_, origin)->GetPlatformNotificationContext();

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(
          &PlatformNotificationContext::
              ReadAllNotificationDataForServiceWorkerRegistration,
          notification_context, origin, service_worker_registration_id,
          base::Bind(&PushMessagingNotificationManager::
                         DidGetNotificationsFromDatabaseIOProxy,
                     weak_factory_.GetWeakPtr(), origin,
                     service_worker_registration_id, message_handled_closure)));
}

// static
void PushMessagingNotificationManager::DidGetNotificationsFromDatabaseIOProxy(
    const base::WeakPtr<PushMessagingNotificationManager>& ui_weak_ptr,
    const GURL& origin,
    int64_t service_worker_registration_id,
    const base::Closure& message_handled_closure,
    bool success,
    const std::vector<NotificationDatabaseData>& data) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(
          &PushMessagingNotificationManager::DidGetNotificationsFromDatabase,
          ui_weak_ptr, origin, service_worker_registration_id,
          message_handled_closure, success, data));
}

void PushMessagingNotificationManager::DidGetNotificationsFromDatabase(
    const GURL& origin,
    int64_t service_worker_registration_id,
    const base::Closure& message_handled_closure,
    bool success,
    const std::vector<NotificationDatabaseData>& data) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // TODO(johnme): Hiding an existing notification should also count as a useful
  // user-visible action done in response to a push message - but make sure that
  // sending two messages in rapid succession which show then hide a
  // notification doesn't count.
  int notification_count = success ? data.size() : 0;
  bool notification_shown = notification_count > 0;
  bool notification_needed = true;

  // Sites with a currently visible tab don't need to show notifications.
#if defined(OS_ANDROID)
  for (auto it = TabModelList::begin(); it != TabModelList::end(); ++it) {
    Profile* profile = (*it)->GetProfile();
    WebContents* active_web_contents = (*it)->GetActiveWebContents();
#else
  for (auto* browser : *BrowserList::GetInstance()) {
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
    for (const auto& notification_database_data : data) {
      if (notification_database_data.notification_data.tag !=
          kPushMessagingForcedNotificationTag)
        continue;

      PlatformNotificationServiceImpl::GetInstance()
          ->ClosePersistentNotification(
              profile_, notification_database_data.notification_id);
      break;
    }
  }

  if (notification_needed && !notification_shown) {
    // If the worker needed to show a notification and didn't, see if a silent
    // push was allowed.
    budget_database_.SpendBudget(
        url::Origin::Create(origin),
        base::BindOnce(&PushMessagingNotificationManager::ProcessSilentPush,
                       weak_factory_.GetWeakPtr(), origin,
                       service_worker_registration_id,
                       message_handled_closure));
    return;
  }

  if (notification_needed && notification_shown) {
    RecordUserVisibleStatus(
        content::mojom::PushUserVisibleStatus::REQUIRED_AND_SHOWN);
  } else if (!notification_needed && !notification_shown) {
    RecordUserVisibleStatus(
        content::mojom::PushUserVisibleStatus::NOT_REQUIRED_AND_NOT_SHOWN);
  } else {
    RecordUserVisibleStatus(
        content::mojom::PushUserVisibleStatus::NOT_REQUIRED_BUT_SHOWN);
  }

  message_handled_closure.Run();
}

bool PushMessagingNotificationManager::IsTabVisible(
    Profile* profile,
    WebContents* active_web_contents,
    const GURL& origin) {
  if (!active_web_contents || !active_web_contents->GetMainFrame())
    return false;

  // Don't leak information from other profiles.
  if (profile != profile_)
    return false;

  // Ignore minimized windows.
  switch (active_web_contents->GetMainFrame()->GetVisibilityState()) {
    case blink::mojom::PageVisibilityState::kHidden:
    case blink::mojom::PageVisibilityState::kPrerender:
      return false;
    case blink::mojom::PageVisibilityState::kVisible:
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

  return visible_url.GetOrigin() == origin;
}

void PushMessagingNotificationManager::ProcessSilentPush(
    const GURL& origin,
    int64_t service_worker_registration_id,
    const base::Closure& message_handled_closure,
    bool silent_push_allowed) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // If the origin was allowed to issue a silent push, just return.
  if (silent_push_allowed) {
    RecordUserVisibleStatus(content::mojom::PushUserVisibleStatus::
                                REQUIRED_BUT_NOT_SHOWN_USED_GRACE);
    message_handled_closure.Run();
    return;
  }

  RecordUserVisibleStatus(content::mojom::PushUserVisibleStatus::
                              REQUIRED_BUT_NOT_SHOWN_GRACE_EXCEEDED);
  rappor::SampleDomainAndRegistryFromGURL(
      g_browser_process->rappor_service(),
      "PushMessaging.GenericNotificationShown.Origin", origin);

  // The site failed to show a notification when one was needed, and they don't
  // have enough budget to cover the cost of suppressing, so we will show a
  // generic notification.
  NotificationDatabaseData database_data =
      CreateDatabaseData(origin, service_worker_registration_id);
  scoped_refptr<PlatformNotificationContext> notification_context =
      GetStoragePartition(profile_, origin)->GetPlatformNotificationContext();
  int64_t next_persistent_notification_id =
      PlatformNotificationServiceImpl::GetInstance()
          ->ReadNextPersistentNotificationId(profile_);
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&PlatformNotificationContext::WriteNotificationData,
                     notification_context, next_persistent_notification_id,
                     service_worker_registration_id, origin, database_data,
                     base::Bind(&PushMessagingNotificationManager::
                                    DidWriteNotificationDataIOProxy,
                                weak_factory_.GetWeakPtr(), origin,
                                database_data.notification_data,
                                message_handled_closure)));
}

// static
void PushMessagingNotificationManager::DidWriteNotificationDataIOProxy(
    const base::WeakPtr<PushMessagingNotificationManager>& ui_weak_ptr,
    const GURL& origin,
    const blink::PlatformNotificationData& notification_data,
    const base::Closure& message_handled_closure,
    bool success,
    const std::string& notification_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(
          &PushMessagingNotificationManager::DidWriteNotificationData,
          ui_weak_ptr, origin, notification_data, message_handled_closure,
          success, notification_id));
}

void PushMessagingNotificationManager::DidWriteNotificationData(
    const GURL& origin,
    const blink::PlatformNotificationData& notification_data,
    const base::Closure& message_handled_closure,
    bool success,
    const std::string& notification_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!success) {
    DLOG(ERROR) << "Writing forced notification to database should not fail";
    message_handled_closure.Run();
    return;
  }

  // Do not pass service worker scope. The origin will be used instead of the
  // service worker scope to determine whether a notification should be
  // attributed to a WebAPK on Android. This is OK because this code path is hit
  // rarely.
  PlatformNotificationServiceImpl::GetInstance()->DisplayPersistentNotification(
      profile_, notification_id, GURL() /* service_worker_scope */, origin,
      notification_data, blink::NotificationResources());

  message_handled_closure.Run();
}
