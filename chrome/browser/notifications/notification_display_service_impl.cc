// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_display_service_impl.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/observer_list.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/notifications/non_persistent_notification_handler.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/notifications/persistent_notification_handler.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/tailored_security/notification_handler_desktop.h"
#include "chrome/browser/updates/announcement_notification/announcement_notification_handler.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/buildflags/buildflags.h"
#include "ui/message_center/public/cpp/notification.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/api/notifications/extension_notification_handler.h"
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_WIN)
#include "chrome/browser/send_tab_to_self/desktop_notification_handler.h"
#include "chrome/browser/sharing/sharing_notification_handler.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/nearby_sharing/nearby_notification_handler.h"
#include "chrome/browser/nearby_sharing/nearby_sharing_service_factory.h"
#endif

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/notifications/muted_notification_handler.h"
#include "chrome/browser/notifications/screen_capture_notification_blocker.h"
#endif

namespace {

void OperationCompleted() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

}  // namespace

// static
NotificationDisplayServiceImpl* NotificationDisplayServiceImpl::GetForProfile(
    Profile* profile) {
  return static_cast<NotificationDisplayServiceImpl*>(
      NotificationDisplayServiceFactory::GetForProfile(profile));
}

// static
void NotificationDisplayServiceImpl::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
#if BUILDFLAG(IS_LINUX)
  registry->RegisterBooleanPref(prefs::kAllowSystemNotifications, true);
#endif
}

NotificationDisplayServiceImpl::NotificationDisplayServiceImpl(Profile* profile)
    : profile_(profile) {
  // TODO(peter): Move these to the NotificationDisplayServiceFactory.
  if (profile_) {
    AddNotificationHandler(
        NotificationHandler::Type::WEB_NON_PERSISTENT,
        std::make_unique<NonPersistentNotificationHandler>());
    AddNotificationHandler(NotificationHandler::Type::WEB_PERSISTENT,
                           std::make_unique<PersistentNotificationHandler>());

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_WIN)
    AddNotificationHandler(
        NotificationHandler::Type::SEND_TAB_TO_SELF,
        std::make_unique<send_tab_to_self::DesktopNotificationHandler>(
            profile_));
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_WIN)
    AddNotificationHandler(
        NotificationHandler::Type::TAILORED_SECURITY,
        std::make_unique<safe_browsing::TailoredSecurityNotificationHandler>());
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
    AddNotificationHandler(
        NotificationHandler::Type::EXTENSION,
        std::make_unique<extensions::ExtensionNotificationHandler>());
#endif

#if !BUILDFLAG(IS_ANDROID)
    AddNotificationHandler(NotificationHandler::Type::SHARING,
                           std::make_unique<SharingNotificationHandler>());
    AddNotificationHandler(NotificationHandler::Type::ANNOUNCEMENT,
                           std::make_unique<AnnouncementNotificationHandler>());

    auto screen_capture_blocker =
        std::make_unique<ScreenCaptureNotificationBlocker>(this);
    AddNotificationHandler(NotificationHandler::Type::NOTIFICATIONS_MUTED,
                           std::make_unique<MutedNotificationHandler>(
                               screen_capture_blocker.get()));
    notification_queue_.AddNotificationBlocker(
        std::move(screen_capture_blocker));
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
    if (NearbySharingServiceFactory::IsNearbyShareSupportedForBrowserContext(
            profile_)) {
      AddNotificationHandler(NotificationHandler::Type::NEARBY_SHARE,
                             std::make_unique<NearbyNotificationHandler>());
    }
#endif
  }

  bridge_delegator_ = std::make_unique<NotificationPlatformBridgeDelegator>(
      profile_,
      base::BindOnce(
          &NotificationDisplayServiceImpl::OnNotificationPlatformBridgeReady,
          weak_factory_.GetWeakPtr()));
}

NotificationDisplayServiceImpl::~NotificationDisplayServiceImpl() {
  for (auto& obs : observers_)
    obs.OnNotificationDisplayServiceDestroyed(this);
}

void NotificationDisplayServiceImpl::ProcessNotificationOperation(
    NotificationOperation operation,
    NotificationHandler::Type notification_type,
    const GURL& origin,
    const std::string& notification_id,
    const std::optional<int>& action_index,
    const std::optional<std::u16string>& reply,
    const std::optional<bool>& by_user) {
  NotificationHandler* handler = GetNotificationHandler(notification_type);
  DCHECK(handler);
  if (!handler) {
    LOG(ERROR) << "Unable to find a handler for "
               << static_cast<int>(notification_type);
    return;
  }

  // TODO(crbug.com/40540804): Plumb this through from the notification platform
  // bridges so they can report completion of the operation as needed.
  base::OnceClosure completed_closure = base::BindOnce(&OperationCompleted);

  switch (operation) {
    case NotificationOperation::kClick:
      handler->OnClick(profile_, origin, notification_id, action_index, reply,
                       std::move(completed_closure));
      break;
    case NotificationOperation::kClose:
      DCHECK(by_user.has_value());
      handler->OnClose(profile_, origin, notification_id, by_user.value(),
                       std::move(completed_closure));
      for (auto& observer : observers_)
        observer.OnNotificationClosed(notification_id);
      break;
    case NotificationOperation::kDisablePermission:
      handler->DisableNotifications(profile_, origin);
      break;
    case NotificationOperation::kSettings:
      handler->OpenSettings(profile_, origin);
      break;
  }
}

void NotificationDisplayServiceImpl::AddNotificationHandler(
    NotificationHandler::Type notification_type,
    std::unique_ptr<NotificationHandler> handler) {
  DCHECK(handler);
  DCHECK_EQ(notification_handlers_.count(notification_type), 0u);
  notification_handlers_[notification_type] = std::move(handler);
}

NotificationHandler* NotificationDisplayServiceImpl::GetNotificationHandler(
    NotificationHandler::Type notification_type) {
  auto found = notification_handlers_.find(notification_type);
  if (found != notification_handlers_.end())
    return found->second.get();
  return nullptr;
}

void NotificationDisplayServiceImpl::Shutdown() {
  bridge_delegator_->DisplayServiceShutDown();
}

void NotificationDisplayServiceImpl::Display(
    NotificationHandler::Type notification_type,
    const message_center::Notification& notification,
    std::unique_ptr<NotificationCommon::Metadata> metadata) {
  // TODO(estade): in the future, the reverse should also be true: a
  // non-TRANSIENT type implies no delegate.
  if (notification_type == NotificationHandler::Type::TRANSIENT)
    DCHECK(notification.delegate());

  CHECK(profile_ || notification_type == NotificationHandler::Type::TRANSIENT);

  if (!bridge_delegator_initialized_) {
    actions_.push(base::BindOnce(&NotificationDisplayServiceImpl::Display,
                                 weak_factory_.GetWeakPtr(), notification_type,
                                 notification, std::move(metadata)));
    return;
  }

  for (auto& observer : observers_)
    observer.OnNotificationDisplayed(notification, metadata.get());

  if (notification_queue_.ShouldEnqueueNotification(notification_type,
                                                    notification)) {
    notification_queue_.EnqueueNotification(notification_type, notification,
                                            std::move(metadata));
  } else {
    bridge_delegator_->Display(notification_type, notification,
                               std::move(metadata));
  }

  NotificationHandler* handler = GetNotificationHandler(notification_type);
  if (handler)
    handler->OnShow(profile_, notification.id());
}

void NotificationDisplayServiceImpl::Close(
    NotificationHandler::Type notification_type,
    const std::string& notification_id) {
  CHECK(profile_ || notification_type == NotificationHandler::Type::TRANSIENT);

  if (!bridge_delegator_initialized_) {
    actions_.push(base::BindOnce(&NotificationDisplayServiceImpl::Close,
                                 weak_factory_.GetWeakPtr(), notification_type,
                                 notification_id));
    return;
  }

  notification_queue_.RemoveQueuedNotification(notification_id);

  bridge_delegator_->Close(notification_type, notification_id);
}

void NotificationDisplayServiceImpl::GetDisplayed(
    DisplayedNotificationsCallback callback) {
  if (!bridge_delegator_initialized_) {
    actions_.push(base::BindOnce(&NotificationDisplayServiceImpl::GetDisplayed,
                                 weak_factory_.GetWeakPtr(),
                                 std::move(callback)));
    return;
  }

  bridge_delegator_->GetDisplayed(
      base::BindOnce(&NotificationDisplayServiceImpl::OnGetDisplayed,
                     weak_factory_.GetWeakPtr(), /*origin=*/std::nullopt,
                     std::move(callback)));
}

void NotificationDisplayServiceImpl::GetDisplayedForOrigin(
    const GURL& origin,
    DisplayedNotificationsCallback callback) {
  if (!bridge_delegator_initialized_) {
    actions_.push(base::BindOnce(
        &NotificationDisplayServiceImpl::GetDisplayedForOrigin,
        weak_factory_.GetWeakPtr(), origin, std::move(callback)));
    return;
  }

  bridge_delegator_->GetDisplayedForOrigin(
      origin,
      base::BindOnce(&NotificationDisplayServiceImpl::OnGetDisplayed,
                     weak_factory_.GetWeakPtr(), origin, std::move(callback)));
}

void NotificationDisplayServiceImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void NotificationDisplayServiceImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

// Callback to run once the profile has been loaded in order to perform a
// given |operation| in a notification.
void NotificationDisplayServiceImpl::ProfileLoadedCallback(
    NotificationOperation operation,
    NotificationHandler::Type notification_type,
    const GURL& origin,
    const std::string& notification_id,
    const std::optional<int>& action_index,
    const std::optional<std::u16string>& reply,
    const std::optional<bool>& by_user,
    Profile* profile) {
  base::UmaHistogramBoolean("Notifications.LoadProfileResult",
                            profile != nullptr);
  if (!profile) {
    LOG(WARNING) << "Profile not loaded correctly";
    return;
  }

  NotificationDisplayServiceImpl* display_service =
      NotificationDisplayServiceImpl::GetForProfile(profile);
  display_service->ProcessNotificationOperation(operation, notification_type,
                                                origin, notification_id,
                                                action_index, reply, by_user);
}

void NotificationDisplayServiceImpl::SetBlockersForTesting(
    NotificationDisplayQueue::NotificationBlockers blockers) {
  notification_queue_.SetNotificationBlockers(std::move(blockers));
}

void NotificationDisplayServiceImpl::
    SetNotificationPlatformBridgeDelegatorForTesting(
        std::unique_ptr<NotificationPlatformBridgeDelegator> bridge_delegator) {
  bridge_delegator_ = std::move(bridge_delegator);
  OnNotificationPlatformBridgeReady();
}

void NotificationDisplayServiceImpl::OverrideNotificationHandlerForTesting(
    NotificationHandler::Type notification_type,
    std::unique_ptr<NotificationHandler> handler) {
  DCHECK(handler);
  DCHECK_EQ(1u, notification_handlers_.count(notification_type));
  notification_handlers_[notification_type] = std::move(handler);
}

void NotificationDisplayServiceImpl::OnNotificationPlatformBridgeReady() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  bridge_delegator_initialized_ = true;

  // Flush any pending actions that have yet to execute.
  while (!actions_.empty()) {
    std::move(actions_.front()).Run();
    actions_.pop();
  }
}

void NotificationDisplayServiceImpl::OnGetDisplayed(
    std::optional<GURL> origin,
    DisplayedNotificationsCallback callback,
    std::set<std::string> notification_ids,
    bool supports_synchronization) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::set<std::string> queued =
      origin.has_value()
          ? notification_queue_.GetQueuedNotificationIdsForOrigin(*origin)
          : notification_queue_.GetQueuedNotificationIds();
  notification_ids.insert(queued.begin(), queued.end());

  std::move(callback).Run(std::move(notification_ids),
                          supports_synchronization);
}
