// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/platform_notification_service_impl.h"

#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/metrics/ukm_background_recorder_service.h"
#include "chrome/browser/notifications/metrics/notification_metrics_logger.h"
#include "chrome/browser/notifications/metrics/notification_metrics_logger_factory.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_context.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/platform_notification_context.h"
#include "content/public/browser/storage_partition.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/public/common/notifications/notification_resources.h"
#include "third_party/blink/public/common/notifications/platform_notification_data.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "ui/message_center/public/cpp/notifier_id.h"
#include "url/origin.h"

#if !defined(OS_ANDROID)
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#endif

using content::BrowserContext;
using content::BrowserThread;
using content::NotificationDatabaseData;
using message_center::NotifierId;

namespace {

// Whether a web notification should be displayed when chrome is in full
// screen mode.
static bool ShouldDisplayWebNotificationOnFullScreen(Profile* profile,
                                                     const GURL& origin) {
#if defined(OS_ANDROID)
  NOTIMPLEMENTED();
  return false;
#else
  // Check to see if this notification comes from a webpage that is displaying
  // fullscreen content.
  for (auto* browser : *BrowserList::GetInstance()) {
    // Only consider the browsers for the profile that created the notification
    if (browser->profile() != profile)
      continue;

    content::WebContents* active_contents =
        browser->tab_strip_model()->GetActiveWebContents();
    if (!active_contents)
      continue;

    // Check to see if
    //  (a) the active tab in the browser shares its origin with the
    //      notification.
    //  (b) the browser is fullscreen
    //  (c) the browser has focus.
    if (active_contents->GetURL().GetOrigin() == origin &&
        browser->exclusive_access_manager()->context()->IsFullscreen() &&
        browser->window()->IsActive()) {
      return true;
    }
  }
#endif

  return false;
}

// Records the total number of deleted notifications after all storage
// partitions are done and called OnDeleted. Uses the ref count to keep track
// of pending callbacks.
class RevokeDeleteCountRecorder
    : public base::RefCounted<RevokeDeleteCountRecorder> {
 public:
  RevokeDeleteCountRecorder() : total_deleted_count_(0) {}

  void OnDeleted(bool success, size_t deleted_count) {
    total_deleted_count_ += deleted_count;
  }

 private:
  friend class base::RefCounted<RevokeDeleteCountRecorder>;

  ~RevokeDeleteCountRecorder() {
    UMA_HISTOGRAM_COUNTS_100("Notifications.Permissions.RevokeDeleteCount",
                             total_deleted_count_);
  }

  size_t total_deleted_count_;

  DISALLOW_COPY_AND_ASSIGN(RevokeDeleteCountRecorder);
};

}  // namespace

// static
void PlatformNotificationServiceImpl::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  // The first persistent ID is registered as 10000 rather than 1 to prevent the
  // reuse of persistent notification IDs, which must be unique. Reuse of
  // notification IDs may occur as they were previously stored in a different
  // data store.
  registry->RegisterIntegerPref(prefs::kNotificationNextPersistentId, 10000);

  // Store the next notification trigger time for each profile. If none is set,
  // this will default to base::Time::Max.
  registry->RegisterTimePref(prefs::kNotificationNextTriggerTime,
                             base::Time::Max());
}

PlatformNotificationServiceImpl::PlatformNotificationServiceImpl(
    Profile* profile)
    : profile_(profile),
      trigger_scheduler_(NotificationTriggerScheduler::Create()) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(profile_);
  HostContentSettingsMapFactory::GetForProfile(profile_)->AddObserver(this);
}

PlatformNotificationServiceImpl::~PlatformNotificationServiceImpl() = default;

void PlatformNotificationServiceImpl::Shutdown() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  HostContentSettingsMapFactory::GetForProfile(profile_)->RemoveObserver(this);
  // Clear the profile as we're not supposed to use it anymore.
  profile_ = nullptr;
}

void PlatformNotificationServiceImpl::OnContentSettingChanged(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    const std::string& resource_identifier) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (content_type != ContentSettingsType::NOTIFICATIONS)
    return;

  auto recorder = base::MakeRefCounted<RevokeDeleteCountRecorder>();
  content::BrowserContext::ForEachStoragePartition(
      profile_,
      base::BindRepeating(
          [](scoped_refptr<RevokeDeleteCountRecorder> recorder,
             content::StoragePartition* partition) {
            partition->GetPlatformNotificationContext()
                ->DeleteAllNotificationDataForBlockedOrigins(base::BindOnce(
                    &RevokeDeleteCountRecorder::OnDeleted, recorder));
          },
          recorder));
}

bool PlatformNotificationServiceImpl::WasClosedProgrammatically(
    const std::string& notification_id) {
  return closed_notifications_.erase(notification_id) != 0;
}

// TODO(awdf): Rename to DisplayNonPersistentNotification (Similar for Close)
void PlatformNotificationServiceImpl::DisplayNotification(
    const std::string& notification_id,
    const GURL& origin,
    const blink::PlatformNotificationData& notification_data,
    const blink::NotificationResources& notification_resources) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Posted tasks can request notifications to be added, which would cause a
  // crash (see |ScopedKeepAlive|). We just do nothing here, the user would not
  // see the notification anyway, since we are shutting down.
  if (g_browser_process->IsShuttingDown() || !profile_)
    return;

  DCHECK_EQ(0u, notification_data.actions.size());
  DCHECK_EQ(0u, notification_resources.action_icons.size());

  message_center::Notification notification = CreateNotificationFromData(
      origin, notification_id, notification_data, notification_resources);

  NotificationDisplayServiceFactory::GetForProfile(profile_)->Display(
      NotificationHandler::Type::WEB_NON_PERSISTENT, notification,
      /*metadata=*/nullptr);
}

void PlatformNotificationServiceImpl::DisplayPersistentNotification(
    const std::string& notification_id,
    const GURL& service_worker_scope,
    const GURL& origin,
    const blink::PlatformNotificationData& notification_data,
    const blink::NotificationResources& notification_resources) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  closed_notifications_.erase(notification_id);

  // Posted tasks can request notifications to be added, which would cause a
  // crash (see |ScopedKeepAlive|). We just do nothing here, the user would not
  // see the notification anyway, since we are shutting down.
  // TODO(knollr): IsShuttingDown check should not be required anymore, but some
  // tests try to display a notification during shutdown.
  if (g_browser_process->IsShuttingDown() || !profile_)
    return;

  message_center::Notification notification = CreateNotificationFromData(
      origin, notification_id, notification_data, notification_resources);
  auto metadata = std::make_unique<PersistentNotificationMetadata>();
  metadata->service_worker_scope = service_worker_scope;

  NotificationDisplayServiceFactory::GetForProfile(profile_)->Display(
      NotificationHandler::Type::WEB_PERSISTENT, notification,
      std::move(metadata));

  NotificationMetricsLoggerFactory::GetForBrowserContext(profile_)
      ->LogPersistentNotificationShown();
}

void PlatformNotificationServiceImpl::CloseNotification(
    const std::string& notification_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (g_browser_process->IsShuttingDown() || !profile_)
    return;

  NotificationDisplayServiceFactory::GetForProfile(profile_)->Close(
      NotificationHandler::Type::WEB_NON_PERSISTENT, notification_id);
}

void PlatformNotificationServiceImpl::ClosePersistentNotification(
    const std::string& notification_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (g_browser_process->IsShuttingDown() || !profile_)
    return;

  closed_notifications_.insert(notification_id);

  NotificationDisplayServiceFactory::GetForProfile(profile_)->Close(
      NotificationHandler::Type::WEB_PERSISTENT, notification_id);
}

void PlatformNotificationServiceImpl::GetDisplayedNotifications(
    DisplayedNotificationsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (g_browser_process->IsShuttingDown() || !profile_)
    return;

  // Tests will not have a message center.
  if (profile_->AsTestingProfile()) {
    std::set<std::string> displayed_notifications;
    std::move(callback).Run(std::move(displayed_notifications),
                            false /* supports_synchronization */);
    return;
  }
  NotificationDisplayServiceFactory::GetForProfile(profile_)->GetDisplayed(
      std::move(callback));
}

void PlatformNotificationServiceImpl::ScheduleTrigger(base::Time timestamp) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (g_browser_process->IsShuttingDown() || !profile_)
    return;

  PrefService* prefs = profile_->GetPrefs();
  base::Time current_trigger =
      prefs->GetTime(prefs::kNotificationNextTriggerTime);

  if (current_trigger > timestamp)
    prefs->SetTime(prefs::kNotificationNextTriggerTime, timestamp);

  trigger_scheduler_->ScheduleTrigger(timestamp);
}

base::Time PlatformNotificationServiceImpl::ReadNextTriggerTimestamp() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (g_browser_process->IsShuttingDown() || !profile_)
    return base::Time::Max();

  PrefService* prefs = profile_->GetPrefs();
  return prefs->GetTime(prefs::kNotificationNextTriggerTime);
}

int64_t PlatformNotificationServiceImpl::ReadNextPersistentNotificationId() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (g_browser_process->IsShuttingDown() || !profile_)
    return 0;

  PrefService* prefs = profile_->GetPrefs();

  int64_t current_id = prefs->GetInteger(prefs::kNotificationNextPersistentId);
  int64_t next_id = current_id + 1;

  prefs->SetInteger(prefs::kNotificationNextPersistentId, next_id);
  return next_id;
}

void PlatformNotificationServiceImpl::RecordNotificationUkmEvent(
    const NotificationDatabaseData& data) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (g_browser_process->IsShuttingDown() || !profile_)
    return;

  // Only record the event if a user explicitly interacted with the notification
  // to close it.
  if (data.closed_reason != NotificationDatabaseData::ClosedReason::USER &&
      data.num_clicks == 0 && data.num_action_button_clicks == 0) {
    return;
  }

  // Check if this event can be recorded via UKM.
  auto* ukm_background_service =
      ukm::UkmBackgroundRecorderFactory::GetForProfile(profile_);
  ukm_background_service->GetBackgroundSourceIdIfAllowed(
      url::Origin::Create(data.origin),
      base::BindOnce(&PlatformNotificationServiceImpl::DidGetBackgroundSourceId,
                     std::move(ukm_recorded_closure_for_testing_), data));
}

NotificationTriggerScheduler*
PlatformNotificationServiceImpl::GetNotificationTriggerScheduler() {
  return trigger_scheduler_.get();
}

// static
void PlatformNotificationServiceImpl::DidGetBackgroundSourceId(
    base::OnceClosure recorded_closure,
    const content::NotificationDatabaseData& data,
    base::Optional<ukm::SourceId> source_id) {
  // This background event did not meet the requirements for the UKM service.
  if (!source_id)
    return;

  ukm::builders::Notification builder(*source_id);

  int64_t time_until_first_click_millis =
      data.time_until_first_click_millis.has_value()
          ? data.time_until_first_click_millis.value().InMilliseconds()
          : -1;

  int64_t time_until_last_click_millis =
      data.time_until_last_click_millis.has_value()
          ? data.time_until_last_click_millis.value().InMilliseconds()
          : -1;

  int64_t time_until_close_millis =
      data.time_until_close_millis.has_value()
          ? data.time_until_close_millis.value().InMilliseconds()
          : -1;

  // TODO(yangsharon):Add did_user_open_settings field and update here.
  builder.SetClosedReason(static_cast<int>(data.closed_reason))
      .SetDidReplaceAnotherNotification(data.replaced_existing_notification)
      .SetHasBadge(!data.notification_data.badge.is_empty())
      .SetHasIcon(!data.notification_data.icon.is_empty())
      .SetHasImage(!data.notification_data.image.is_empty())
      .SetHasRenotify(data.notification_data.renotify)
      .SetHasTag(!data.notification_data.tag.empty())
      .SetIsSilent(data.notification_data.silent)
      .SetNumActions(data.notification_data.actions.size())
      .SetNumActionButtonClicks(data.num_action_button_clicks)
      .SetNumClicks(data.num_clicks)
      .SetRequireInteraction(data.notification_data.require_interaction)
      .SetTimeUntilClose(time_until_close_millis)
      .SetTimeUntilFirstClick(time_until_first_click_millis)
      .SetTimeUntilLastClick(time_until_last_click_millis)
      .Record(ukm::UkmRecorder::Get());

  if (recorded_closure)
    std::move(recorded_closure).Run();
}

message_center::Notification
PlatformNotificationServiceImpl::CreateNotificationFromData(
    const GURL& origin,
    const std::string& notification_id,
    const blink::PlatformNotificationData& notification_data,
    const blink::NotificationResources& notification_resources) const {
  DCHECK_EQ(notification_data.actions.size(),
            notification_resources.action_icons.size());

  message_center::RichNotificationData optional_fields;

  optional_fields.settings_button_handler =
      message_center::SettingsButtonHandler::INLINE;

  // TODO(peter): Handle different screen densities instead of always using the
  // 1x bitmap - crbug.com/585815.
  message_center::Notification notification(
      message_center::NOTIFICATION_TYPE_SIMPLE, notification_id,
      notification_data.title, notification_data.body,
      gfx::Image::CreateFrom1xBitmap(notification_resources.notification_icon),
      base::UTF8ToUTF16(origin.host()), origin,
      message_center::NotifierId(origin), optional_fields,
      nullptr /* delegate */);

  notification.set_context_message(DisplayNameForContextMessage(origin));
  notification.set_vibration_pattern(notification_data.vibration_pattern);
  notification.set_timestamp(notification_data.timestamp);
  notification.set_renotify(notification_data.renotify);
  notification.set_silent(notification_data.silent);
  if (ShouldDisplayWebNotificationOnFullScreen(profile_, origin)) {
    notification.set_fullscreen_visibility(
        message_center::FullscreenVisibility::OVER_USER);
  }

  if (!notification_resources.image.drawsNothing()) {
    notification.set_type(message_center::NOTIFICATION_TYPE_IMAGE);
    notification.set_image(
        gfx::Image::CreateFrom1xBitmap(notification_resources.image));
  }

  // TODO(peter): Handle different screen densities instead of always using the
  // 1x bitmap - crbug.com/585815.
  notification.set_small_image(
      gfx::Image::CreateFrom1xBitmap(notification_resources.badge));

  // Developer supplied action buttons.
  std::vector<message_center::ButtonInfo> buttons;
  for (size_t i = 0; i < notification_data.actions.size(); ++i) {
    const blink::PlatformNotificationAction& action =
        notification_data.actions[i];
    message_center::ButtonInfo button(action.title);
    // TODO(peter): Handle different screen densities instead of always using
    // the 1x bitmap - crbug.com/585815.
    button.icon =
        gfx::Image::CreateFrom1xBitmap(notification_resources.action_icons[i]);
    if (action.type == blink::PLATFORM_NOTIFICATION_ACTION_TYPE_TEXT) {
      button.placeholder = action.placeholder.as_optional_string16().value_or(
          l10n_util::GetStringUTF16(IDS_NOTIFICATION_REPLY_PLACEHOLDER));
    }
    buttons.push_back(button);
  }
  notification.set_buttons(buttons);

  // On desktop, notifications with require_interaction==true stay on-screen
  // rather than minimizing to the notification center after a timeout.
  // On mobile, this is ignored (notifications are minimized at all times).
  if (notification_data.require_interaction)
    notification.set_never_timeout(true);

  return notification;
}

base::string16 PlatformNotificationServiceImpl::DisplayNameForContextMessage(
    const GURL& origin) const {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  // If the source is an extension, lookup the display name.
  if (origin.SchemeIs(extensions::kExtensionScheme)) {
    const extensions::Extension* extension =
        extensions::ExtensionRegistry::Get(profile_)->GetExtensionById(
            origin.host(), extensions::ExtensionRegistry::EVERYTHING);
    DCHECK(extension);

    return base::UTF8ToUTF16(extension->name());
  }
#endif

  return base::string16();
}
