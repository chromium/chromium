// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_platform_bridge_delegator.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/notification_display_service_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

#if BUILDFLAG(ENABLE_CHROME_NOTIFICATIONS)
#include "chrome/browser/notifications/notification_platform_bridge_message_center.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/notifications/notification_platform_bridge_win.h"
#endif

namespace {

// Returns if the current platform has system notifications enabled.
// Platforms behave as follows:
//
//   * Android, Chrome OS
//     Always uses system notifications.
//
//   * Windows before 10 RS4 (incl. Win8 & Win7)
//     Always uses message center.
//
//   * Mac OS X, Linux, Windows 10 RS4+
//     Uses system notifications by default, but can fall back to the message
//     center if features::kSystemNotifications is disabled or initialization
//     fails. Linux additionally checks if prefs::kAllowSystemNotifications is
//     disabled and falls back to the message center if so.
//
// Please try to keep this comment up to date when changing behaviour on one of
// the platforms supported by the browser.
bool SystemNotificationsEnabled(Profile* profile) {
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
  return true;
#elif BUILDFLAG(IS_WIN)
  return NotificationPlatformBridgeWin::SystemNotificationEnabled();
#else
#if BUILDFLAG(IS_LINUX)
  if (profile) {
    // Prefs take precedence over flags.
    PrefService* prefs = profile->GetPrefs();
    if (!prefs->GetBoolean(prefs::kAllowSystemNotifications)) {
      return false;
    }
  }
#endif  // BUILDFLAG(IS_LINUX)
  return base::FeatureList::IsEnabled(features::kNativeNotifications) &&
         base::FeatureList::IsEnabled(features::kSystemNotifications);
#endif  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
}

NotificationPlatformBridge* GetSystemNotificationPlatformBridge(
    Profile* profile) {
  if (SystemNotificationsEnabled(profile))
    return g_browser_process->notification_platform_bridge();

  // The platform does not support, or has not enabled, system notifications.
  return nullptr;
}

// Returns the NotificationPlatformBridge to use for the message center. May be
// a nullptr for platforms where the message center is not available.
NotificationPlatformBridge* GetMessageCenterBridge() {
#if BUILDFLAG(ENABLE_CHROME_NOTIFICATIONS)
  return NotificationPlatformBridgeMessageCenter::Get();
#else
  return nullptr;
#endif
}

}  // namespace

NotificationPlatformBridgeDelegator::NotificationPlatformBridgeDelegator(
    Profile* profile,
    base::OnceClosure ready_callback)
    : profile_(profile),
      message_center_bridge_(GetMessageCenterBridge()),
      system_bridge_(GetSystemNotificationPlatformBridge(profile_)),
      ready_callback_(std::move(ready_callback)) {
  // Initialize the |system_bridge_| if system notifications are available,
  // otherwise signal that the bridge could not be initialized.
  if (system_bridge_) {
    system_bridge_->SetReadyCallback(
        base::BindOnce(&NotificationPlatformBridgeDelegator::
                           OnSystemNotificationPlatformBridgeReady,
                       weak_factory_.GetWeakPtr()));
  } else {
    OnSystemNotificationPlatformBridgeReady(/*success=*/false);
  }
}

NotificationPlatformBridgeDelegator::~NotificationPlatformBridgeDelegator() =
    default;

void NotificationPlatformBridgeDelegator::Display(
    NotificationHandler::Type notification_type,
    const message_center::Notification& notification,
    std::unique_ptr<NotificationCommon::Metadata> metadata) {
  NotificationPlatformBridge* bridge = GetBridgeForType(notification_type);
  DCHECK(bridge);
  bridge->Display(notification_type, profile_, notification,
                  std::move(metadata));
}

void NotificationPlatformBridgeDelegator::Close(
    NotificationHandler::Type notification_type,
    const std::string& notification_id) {
  NotificationPlatformBridge* bridge = GetBridgeForType(notification_type);
  DCHECK(bridge);
  bridge->Close(profile_, notification_id);
}

void NotificationPlatformBridgeDelegator::GetDisplayed(
    GetDisplayedNotificationsCallback callback) const {
  // TODO(crbug.com/40788519): We currently only query one of the bridges for
  // displayed notifications which may not return TRANSIENT style ones. Ideally
  // there would be only one bridge to query from.
  NotificationPlatformBridge* bridge =
      system_bridge_ ? system_bridge_.get() : message_center_bridge_.get();
  DCHECK(bridge);
  bridge->GetDisplayed(profile_, std::move(callback));
}

void NotificationPlatformBridgeDelegator::GetDisplayedForOrigin(
    const GURL& origin,
    GetDisplayedNotificationsCallback callback) const {
  // TODO(crbug.com/40788519): We currently only query one of the bridges for
  // displayed notifications which may not return TRANSIENT style ones. Ideally
  // there would be only one bridge to query from.
  NotificationPlatformBridge* bridge =
      system_bridge_ ? system_bridge_.get() : message_center_bridge_.get();
  DCHECK(bridge);
  bridge->GetDisplayedForOrigin(profile_, origin, std::move(callback));
}

void NotificationPlatformBridgeDelegator::DisplayServiceShutDown() {
  if (message_center_bridge_)
    message_center_bridge_->DisplayServiceShutDown(profile_);
  if (system_bridge_)
    system_bridge_->DisplayServiceShutDown(profile_);
}

NotificationPlatformBridge*
NotificationPlatformBridgeDelegator::GetBridgeForType(
    NotificationHandler::Type type) {
  // Prefer the system bridge if available and it can handle |type|.
  if (system_bridge_ && NotificationPlatformBridge::CanHandleType(type))
    return system_bridge_;
  return message_center_bridge_;
}

void NotificationPlatformBridgeDelegator::
    OnSystemNotificationPlatformBridgeReady(bool success) {
  if (!success) {
    // Fall back to the message center if initialization failed. Initialization
    // must always succeed on platforms where the message center is unavailable.
    DCHECK(message_center_bridge_);
    system_bridge_ = nullptr;
  }

  base::UmaHistogramBoolean("Notifications.UsingSystemNotificationCenter",
                            system_bridge_ != nullptr);

  if (ready_callback_)
    std::move(ready_callback_).Run();
}
