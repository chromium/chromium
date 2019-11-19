// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_display_service_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/non_persistent_notification_handler.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/notifications/notification_platform_bridge.h"
#include "chrome/browser/notifications/persistent_notification_handler.h"
#include "chrome/browser/permissions/permission_request_notification_handler.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sharing/sharing_notification_handler.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/ui_base_features.h"
#include "ui/message_center/public/cpp/notification.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/api/notifications/extension_notification_handler.h"
#endif

#if BUILDFLAG(ENABLE_MESSAGE_CENTER)
#include "chrome/browser/notifications/notification_platform_bridge_message_center.h"
#endif

#if defined(OS_LINUX) || defined(OS_MACOSX) || defined(OS_WIN)
#include "chrome/browser/send_tab_to_self/desktop_notification_handler.h"
#endif

#if defined(OS_WIN)
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/notifications/notification_platform_bridge_win.h"
#endif

namespace {

// Returns the NotificationPlatformBridge to use for the current platform.
// Will return a nullptr for platforms that don't support native notifications.
//
// Platforms behave as follows:
//
//   * Android
//     Always uses native notifications.
//
//   * Mac OS X, Linux, Windows 10 RS1+
//     Uses native notifications by default, but can fall back to the message
//     center if base::kNativeNotifications is disabled or initialization fails.
//
//   * Chrome OS
//     Always uses the message center, either through the message center
//     notification platform bridge when base::kNativeNotifications is disabled,
//     which means the message center runs in-process, or through the Chrome OS
//     specific bridge when the flag is enabled, which displays out-of-process.
//
// Please try to keep this comment up to date when changing behaviour on one of
// the platforms supported by the browser.
NotificationPlatformBridge* GetNativeNotificationPlatformBridge() {
#if BUILDFLAG(ENABLE_NATIVE_NOTIFICATIONS)
#if defined(OS_ANDROID)
  DCHECK(base::FeatureList::IsEnabled(features::kNativeNotifications));
  return g_browser_process->notification_platform_bridge();
#elif defined(OS_WIN)
  if (NotificationPlatformBridgeWin::NativeNotificationEnabled())
    return g_browser_process->notification_platform_bridge();
#elif defined(OS_CHROMEOS)
  return g_browser_process->notification_platform_bridge();
#else
  if (base::FeatureList::IsEnabled(features::kNativeNotifications) &&
      g_browser_process->notification_platform_bridge()) {
    return g_browser_process->notification_platform_bridge();
  }
#endif
#endif  // BUILDFLAG(ENABLE_NATIVE_NOTIFICATIONS)

  // The platform does not support, or has not enabled, native notifications.
  return nullptr;
}

// Returns the NotificationPlatformBridge to use for the message center. May be
// a nullptr for platforms where the message center is not available.
std::unique_ptr<NotificationPlatformBridge> CreateMessageCenterBridge(
    Profile* profile) {
#if BUILDFLAG(ENABLE_MESSAGE_CENTER)
  return std::make_unique<NotificationPlatformBridgeMessageCenter>(profile);
#else
  return nullptr;
#endif
}

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

NotificationDisplayServiceImpl::NotificationDisplayServiceImpl(Profile* profile)
    : profile_(profile),
      message_center_bridge_(CreateMessageCenterBridge(profile)),
      bridge_(GetNativeNotificationPlatformBridge()) {
  // TODO(peter): Move these to the NotificationDisplayServiceFactory.
  if (profile_) {
    AddNotificationHandler(
        NotificationHandler::Type::WEB_NON_PERSISTENT,
        std::make_unique<NonPersistentNotificationHandler>());
    AddNotificationHandler(NotificationHandler::Type::WEB_PERSISTENT,
                           std::make_unique<PersistentNotificationHandler>());

#if defined(OS_LINUX) || defined(OS_MACOSX) || defined(OS_WIN)
    AddNotificationHandler(
        NotificationHandler::Type::SEND_TAB_TO_SELF,
        std::make_unique<send_tab_to_self::DesktopNotificationHandler>(
            profile_));
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
    AddNotificationHandler(
        NotificationHandler::Type::EXTENSION,
        std::make_unique<extensions::ExtensionNotificationHandler>());
#endif

#if defined(OS_ANDROID)
    AddNotificationHandler(
        NotificationHandler::Type::PERMISSION_REQUEST,
        std::make_unique<PermissionRequestNotificationHandler>());
#endif
#if !defined(OS_ANDROID)
    AddNotificationHandler(NotificationHandler::Type::SHARING,
                           std::make_unique<SharingNotificationHandler>());
#endif
  }

  // Initialize the bridge if native notifications are available, otherwise
  // signal that the bridge could not be initialized.
  if (bridge_) {
    bridge_->SetReadyCallback(base::BindOnce(
        &NotificationDisplayServiceImpl::OnNotificationPlatformBridgeReady,
        weak_factory_.GetWeakPtr()));
  } else {
    OnNotificationPlatformBridgeReady(false /* success */);
  }
}

NotificationDisplayServiceImpl::~NotificationDisplayServiceImpl() = default;

void NotificationDisplayServiceImpl::ProcessNotificationOperation(
    NotificationCommon::Operation operation,
    NotificationHandler::Type notification_type,
    const GURL& origin,
    const std::string& notification_id,
    const base::Optional<int>& action_index,
    const base::Optional<base::string16>& reply,
    const base::Optional<bool>& by_user) {
  NotificationHandler* handler = GetNotificationHandler(notification_type);
  DCHECK(handler);
  if (!handler) {
    LOG(ERROR) << "Unable to find a handler for "
               << static_cast<int>(notification_type);
    return;
  }

  // TODO(crbug.com/766854): Plumb this through from the notification platform
  // bridges so they can report completion of the operation as needed.
  base::OnceClosure completed_closure = base::BindOnce(&OperationCompleted);

  switch (operation) {
    case NotificationCommon::OPERATION_CLICK:
      handler->OnClick(profile_, origin, notification_id, action_index, reply,
                       std::move(completed_closure));
      break;
    case NotificationCommon::OPERATION_CLOSE:
      DCHECK(by_user.has_value());
      handler->OnClose(profile_, origin, notification_id, by_user.value(),
                       std::move(completed_closure));
      break;
    case NotificationCommon::OPERATION_DISABLE_PERMISSION:
      handler->DisableNotifications(profile_, origin);
      break;
    case NotificationCommon::OPERATION_SETTINGS:
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
  if (!bridge_initialized_)
    return;

  if (message_center_bridge_)
    message_center_bridge_->DisplayServiceShutDown(profile_);
  if (bridge_)
    bridge_->DisplayServiceShutDown(profile_);
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

  if (!bridge_initialized_) {
    actions_.push(base::BindOnce(&NotificationDisplayServiceImpl::Display,
                                 weak_factory_.GetWeakPtr(), notification_type,
                                 notification, std::move(metadata)));
    return;
  }

#if BUILDFLAG(ENABLE_NATIVE_NOTIFICATIONS)
  NotificationPlatformBridge* bridge =
      NotificationPlatformBridge::CanHandleType(notification_type)
          ? bridge_
          : message_center_bridge_.get();
  DCHECK(bridge);

  bridge->Display(notification_type, profile_, notification,
                  std::move(metadata));
#endif

  NotificationHandler* handler = GetNotificationHandler(notification_type);
  if (handler)
    handler->OnShow(profile_, notification.id());
}

void NotificationDisplayServiceImpl::Close(
    NotificationHandler::Type notification_type,
    const std::string& notification_id) {
  CHECK(profile_ || notification_type == NotificationHandler::Type::TRANSIENT);

  if (!bridge_initialized_) {
    actions_.push(base::BindOnce(&NotificationDisplayServiceImpl::Close,
                                 weak_factory_.GetWeakPtr(), notification_type,
                                 notification_id));
    return;
  }

#if BUILDFLAG(ENABLE_NATIVE_NOTIFICATIONS)
  NotificationPlatformBridge* bridge =
      NotificationPlatformBridge::CanHandleType(notification_type)
          ? bridge_
          : message_center_bridge_.get();
  DCHECK(bridge);

  bridge->Close(profile_, notification_id);
#endif
}

void NotificationDisplayServiceImpl::GetDisplayed(
    DisplayedNotificationsCallback callback) {
  if (!bridge_initialized_) {
    actions_.push(base::BindOnce(&NotificationDisplayServiceImpl::GetDisplayed,
                                 weak_factory_.GetWeakPtr(),
                                 std::move(callback)));
    return;
  }

  bridge_->GetDisplayed(profile_, std::move(callback));
}

// Callback to run once the profile has been loaded in order to perform a
// given |operation| in a notification.
void NotificationDisplayServiceImpl::ProfileLoadedCallback(
    NotificationCommon::Operation operation,
    NotificationHandler::Type notification_type,
    const GURL& origin,
    const std::string& notification_id,
    const base::Optional<int>& action_index,
    const base::Optional<base::string16>& reply,
    const base::Optional<bool>& by_user,
    Profile* profile) {
  if (!profile) {
    // TODO(miguelg): Add UMA for this condition.
    // Perhaps propagate this through PersistentNotificationStatus.
    LOG(WARNING) << "Profile not loaded correctly";
    return;
  }

  NotificationDisplayServiceImpl* display_service =
      NotificationDisplayServiceImpl::GetForProfile(profile);
  display_service->ProcessNotificationOperation(operation, notification_type,
                                                origin, notification_id,
                                                action_index, reply, by_user);
}

void NotificationDisplayServiceImpl::OnNotificationPlatformBridgeReady(
    bool success) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
#if BUILDFLAG(ENABLE_NATIVE_NOTIFICATIONS) && !defined(OS_CHROMEOS)
  if (base::FeatureList::IsEnabled(features::kNativeNotifications)) {
    UMA_HISTOGRAM_BOOLEAN("Notifications.UsingNativeNotificationCenter",
                          success);
  }
#endif

  if (!success) {
    // Fall back to the message center if initialization failed. Initialization
    // must always succeed on platforms where the message center is unavailable.
    DCHECK(message_center_bridge_);
    bridge_ = message_center_bridge_.get();
  }

  bridge_initialized_ = true;

  // Flush any pending actions that have yet to execute.
  while (!actions_.empty()) {
    std::move(actions_.front()).Run();
    actions_.pop();
  }
}
