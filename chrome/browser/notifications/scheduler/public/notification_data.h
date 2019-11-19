// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_PUBLIC_NOTIFICATION_DATA_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_PUBLIC_NOTIFICATION_DATA_H_

#include <map>
#include <string>
#include <vector>

#include "base/strings/string16.h"
#include "chrome/browser/notifications/scheduler/public/icon_bundle.h"
#include "chrome/browser/notifications/scheduler/public/notification_scheduler_types.h"

namespace notifications {

// Contains data used to display a scheduled notification. All fields will be
// persisted to disk as protobuffer NotificationData. The clients of
// notification scheduler can optionally use the texts or icon in this struct,
// or retrieving the hard coded assets and rewrites the data before notification
// is shown in NotificationSchedulerClient::BeforeShowNotification().
struct NotificationData {
  // Represents a button on the notification UI.
  struct Button {
    Button();
    Button(const Button& other);
    bool operator==(const Button& other) const;
    ~Button();

    // The text associated with the button.
    base::string16 text;

    // The button type.
    ActionButtonType type;

    // The id of the button.
    std::string id;
  };

  using CustomData = std::map<std::string, std::string>;
  NotificationData();
  NotificationData(const NotificationData& other);
  bool operator==(const NotificationData& other) const;
  ~NotificationData();

  // The title of the notification.
  base::string16 title;

  // The body text of the notification.
  base::string16 message;

  // The icons of the notification.
  std::map<IconType, IconBundle> icons;

  // Custom key value pair data associated with each notification. Will be sent
  // back after user interaction.
  CustomData custom_data;

  // A list of buttons on the notification.
  std::vector<Button> buttons;
};

}  // namespace notifications

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_PUBLIC_NOTIFICATION_DATA_H_
