// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/profile_notification.h"

#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/profiles/profile.h"
#include "components/account_id/account_id.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#endif

// static
std::string ProfileNotification::GetProfileNotificationId(
    const std::string& delegate_id,
    ProfileID profile_id) {
  return base::StringPrintf("notification-ui-manager#%p#%s",
                            profile_id,  // Each profile has its unique instance
                                         // including incognito profile.
                            delegate_id.c_str());
}

// static
ProfileNotification::ProfileID ProfileNotification::GetProfileID(
    Profile* profile) {
  return static_cast<ProfileID>(profile);
}

ProfileNotification::ProfileNotification(
    Profile* profile,
    const message_center::Notification& notification,
    NotificationHandler::Type type)
    : profile_(profile),
      profile_id_(GetProfileID(profile)),
      notification_(
          // Uses Notification's copy constructor to assign the message center
          // id, which should be unique for every profile + Notification pair.
          GetProfileNotificationId(notification.id(), GetProfileID(profile)),
          notification),
      original_id_(notification.id()),
      type_(type) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (profile_) {
    notification_.set_profile_id(
        multi_user_util::GetAccountIdFromProfile(profile).GetUserEmail());
  }
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  // TODO(https://crbug.com/2673648): Add a stable identifier to the
  // notification that allows Lacros to be launched into the correct profile if
  // it is not running.

  // On Lacros notifications should not keep the browser alive, as they are
  // persisted by the OS in a hidden tray.
#else
  // These keepalives prevent the browser process from shutting down when
  // the last browser window is closed and there are open notifications. It's
  // not used on Chrome OS as closing the last browser window never shuts down
  // the process or delete the Profile.
  keep_alive_ = std::make_unique<ScopedKeepAlive>(
      KeepAliveOrigin::NOTIFICATION, KeepAliveRestartOption::DISABLED);
  if (profile_ && !profile_->IsOffTheRecord()) {
    // No need to create a keepalive for Incognito profiles. Incognito
    // notifications are cleaned up in
    // NotificationUIManagerImpl::OnProfileWillBeDestroyed().
    profile_keep_alive_ = std::make_unique<ScopedProfileKeepAlive>(
        profile, ProfileKeepAliveOrigin::kNotification);
  }
#endif
}

ProfileNotification::~ProfileNotification() = default;
