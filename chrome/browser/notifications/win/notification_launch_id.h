// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_WIN_NOTIFICATION_LAUNCH_ID_H_
#define CHROME_BROWSER_NOTIFICATIONS_WIN_NOTIFICATION_LAUNCH_ID_H_

#include "base/strings/string16.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "url/gurl.h"

// This class encapsulates the launch id strings that are passed back and forth
// between Chrome and the Windows Action Center and contain the necessary info
// to figure out which notification was activated in the Action Center.
class NotificationLaunchId {
 public:
  NotificationLaunchId();
  NotificationLaunchId(const NotificationLaunchId& other);

  // |notification_id| and |profile_id| must be UTF8 strings.
  NotificationLaunchId(NotificationHandler::Type notification_type,
                       const std::string& notification_id,
                       const std::string& profile_id,
                       bool incognito,
                       const GURL& origin_url);

  // A constructor used to parse an encoded string we get back from the Action
  // Center. Callers must use is_valid() to check if decoding was successful.
  explicit NotificationLaunchId(const std::string& encoded);

  ~NotificationLaunchId() = default;

  NotificationLaunchId& operator=(const NotificationLaunchId& other) = default;

  bool is_valid() const { return is_valid_; }

  std::string Serialize() const;

  void set_button_index(int index) {
    DCHECK(!is_for_context_menu_);
    button_index_ = index;
  }

  void set_is_for_context_menu() {
    DCHECK_EQ(-1, button_index_);
    is_for_context_menu_ = true;
  }

  void set_is_for_dismiss_button() {
    DCHECK_EQ(-1, button_index_);
    is_for_dismiss_button_ = true;
  }

  NotificationHandler::Type notification_type() const {
    DCHECK(is_valid());
    return notification_type_;
  }

  const std::string& notification_id() const {
    DCHECK(is_valid());
    return notification_id_;
  }

  const std::string& profile_id() const {
    DCHECK(is_valid());
    return profile_id_;
  }

  bool incognito() const {
    DCHECK(is_valid());
    return incognito_;
  }

  const GURL& origin_url() const {
    DCHECK(is_valid());
    return origin_url_;
  }

  int button_index() const {
    DCHECK(is_valid());
    return button_index_;
  }

  bool is_for_context_menu() const {
    DCHECK(is_valid());
    return is_for_context_menu_;
  }

  bool is_for_dismiss_button() const {
    DCHECK(is_valid());
    return is_for_dismiss_button_;
  }

  // Extracts the profile ID from |launch_id_str|.
  static std::string GetProfileIdFromLaunchId(
      const base::string16& launch_id_str);

 private:
  // The notification type this launch ID is associated with.
  NotificationHandler::Type notification_type_;

  // The notification id this launch ID is associated with. The string is UTF8.
  std::string notification_id_;

  // The profile id this launch ID is associated with. The string is UTF8.
  std::string profile_id_;

  // A flag indicating if the notification associated with this launch ID is in
  // incognito mode or not.
  bool incognito_ = false;

  // The original URL this launch ID is associated with.
  GURL origin_url_;

  // The button index this launch ID is associated with.
  int button_index_ = -1;

  // A flag indicating if this launch ID is targeting the context menu or not.
  bool is_for_context_menu_ = false;

  // A flag indicating if this launch ID is targeting the dismiss button or not.
  bool is_for_dismiss_button_ = false;

  // A flag indicating if this launch ID is valid.
  bool is_valid_ = false;
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_WIN_NOTIFICATION_LAUNCH_ID_H_
