// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_platform_bridge_delegator.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/check.h"
#include "base/metrics/histogram_functions.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/notification_display_service_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "ui/base/ui_base_features.h"

#if BUILDFLAG(ENABLE_MESSAGE_CENTER)
#include "chrome/browser/notifications/notification_platform_bridge_message_center.h"
#endif

#if defined(OS_WIN)
#include "chrome/browser/notifications/notification_platform_bridge_win.h"
#endif

namespace {

// Returns if the current platform has native notifications enabled.
// Platforms behave as follows:
//
//   * Android, Chrome OS
//     Always uses native notifications.
//
//   * Windows before 10 RS4 (incl. Win8 & Win7)
//     Always uses message center.
//
//   * Mac OS X, Linux, Windows 10 RS4+
//     Uses native notifications by default, but can fall back to the message
//     center if features::kNativeNotifications is disabled or initialization
//     fails. Linux additionally checks if prefs::kAllowNativeNotifications is
//     disabled and falls back to the message center if so.
//
// Please try to keep this comment up to date when changing behaviour on one of
// the platforms supported by the browser.
bool NativeNotificationsEnabled(Profile* profile) {
#if BUILDFLAG(ENABLE_NATIVE_NOTIFICATIONS)
#if defined(OS_CHROMEOS) || defined(OS_ANDROID)
  return true;
#elif defined(OS_WIN)
  return NotificationPlatformBridgeWin::NativeNotificationEnabled();
#elif defined(OS_LINUX)
  if (profile) {
    // Prefs take precedence over flags.
    PrefService* prefs = profile->GetPrefs();
    if (!prefs->GetBoolean(prefs::kAllowNativeNotifications))
      return false;
  }
#endif
  return base::FeatureList::IsEnabled(features::kNativeNotifications);
#endif  // BUILDFLAG(ENABLE_NATIVE_NOTIFICATIONS)
  return false;
}

NotificationPlatformBridge* GetNativeNotificationPlatformBridge(
    Profile* profile) {
  if (NativeNotificationsEnabled(profile))
    return g_browser_process->notification_platform_bridge();

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

}  // namespace

NotificationPlatformBridgeDelegator::NotificationPlatformBridgeDelegator(
    Profile* profile,
    base::OnceClosure ready_callback)
    : profile_(profile),
      message_center_bridge_(CreateMessageCenterBridge(profile_)),
      native_bridge_(GetNativeNotificationPlatformBridge(profile_)),
      ready_callback_(std::move(ready_callback)) {
  // Initialize the |native_bridge_| if native notifications are available,
  // otherwise signal that the bridge could not be initialized.
  if (native_bridge_) {
    native_bridge_->SetReadyCallback(
        base::BindOnce(&NotificationPlatformBridgeDelegator::
                           OnNativeNotificationPlatformBridgeReady,
                       weak_factory_.GetWeakPtr()));
  } else {
    OnNativeNotificationPlatformBridgeReady(/*success=*/false);
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
  // TODO(knollr): Query both bridges to get all notifications.
  NotificationPlatformBridge* bridge =
      native_bridge_ ? native_bridge_ : message_center_bridge_.get();
  DCHECK(bridge);
  bridge->GetDisplayed(profile_, std::move(callback));
}

void NotificationPlatformBridgeDelegator::DisplayServiceShutDown() {
  if (message_center_bridge_)
    message_center_bridge_->DisplayServiceShutDown(profile_);
  if (native_bridge_)
    native_bridge_->DisplayServiceShutDown(profile_);
}

NotificationPlatformBridge*
NotificationPlatformBridgeDelegator::GetBridgeForType(
    NotificationHandler::Type type) {
#if BUILDFLAG(ENABLE_NATIVE_NOTIFICATIONS)
  // Prefer the native bridge if available and it can handle |type|.
  if (native_bridge_ && NotificationPlatformBridge::CanHandleType(type))
    return native_bridge_;
#endif  // BUILDFLAG(ENABLE_NATIVE_NOTIFICATIONS)
  return message_center_bridge_.get();
}

void NotificationPlatformBridgeDelegator::
    OnNativeNotificationPlatformBridgeReady(bool success) {
  if (!success) {
    // Fall back to the message center if initialization failed. Initialization
    // must always succeed on platforms where the message center is unavailable.
    DCHECK(message_center_bridge_);
    native_bridge_ = nullptr;
  }

  base::UmaHistogramBoolean("Notifications.UsingNativeNotificationCenter",
                            native_bridge_ != nullptr);

  if (ready_callback_)
    std::move(ready_callback_).Run();
}
