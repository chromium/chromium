// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_PROFILE_NOTIFICATION_H_
#define CHROME_BROWSER_NOTIFICATIONS_PROFILE_NOTIFICATION_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/notifications/notification_common.h"
#include "ui/message_center/public/cpp/notification.h"

class Profile;
class ScopedKeepAlive;
class ScopedProfileKeepAlive;

// This class keeps a Notification object and its corresponding Profile. It
// permutes the notification's ID to include a profile identifier so that two
// notifications with identical IDs and different source Profiles can be
// distinguished. This is necessary because the MessageCenter as well as native
// notification services have no notion of the profile.
class ProfileNotification {
 public:
  using ProfileID = void*;

  // Returns a string that uniquely identifies a profile + delegate_id pair.
  // The profile_id is used as an identifier to identify a profile instance; it
  // can be null for system notifications. The ID becomes invalid when a profile
  // is destroyed.
  static std::string GetProfileNotificationId(const std::string& delegate_id,
                                              ProfileID profile_id);

  // Convert a profile pointer into an opaque profile id, which can be safely
  // used by NotificationUIManager even after a profile may have been destroyed.
  static ProfileID GetProfileID(Profile* profile);

  ProfileNotification(
      Profile* profile,
      const message_center::Notification& notification,
      NotificationHandler::Type type = NotificationHandler::Type::MAX);
  ProfileNotification(const ProfileNotification&) = delete;
  ProfileNotification& operator=(const ProfileNotification&) = delete;
  ~ProfileNotification();

  Profile* profile() const { return profile_; }
  ProfileID profile_id() const { return profile_id_; }
  const message_center::Notification& notification() const {
    return notification_;
  }
  const std::string& original_id() const { return original_id_; }

  NotificationHandler::Type type() const { return type_; }

 private:
  raw_ptr<Profile> profile_;

  // Used for equality comparision in notification maps.
  ProfileID profile_id_;

  message_center::Notification notification_;

  // The ID as it existed for |notification| before being prepended with a
  // profile identifier.
  std::string original_id_;

  NotificationHandler::Type type_;

  std::unique_ptr<ScopedKeepAlive> keep_alive_;
  std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive_;
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_PROFILE_NOTIFICATION_H_
