// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_NEARBY_NOTIFICATION_DELEGATE_H_
#define CHROME_BROWSER_NEARBY_SHARING_NEARBY_NOTIFICATION_DELEGATE_H_

#include <optional>
#include <string>

// Notification delegate that handles specific click and close events.
class NearbyNotificationDelegate {
 public:
  NearbyNotificationDelegate() = default;
  virtual ~NearbyNotificationDelegate() = default;

  // Called when the user clicks on the notification with |notification_id|.
  // When the click is on the notification itself |action_index| is nullopt.
  // Otherwise |action_index| contains the index of the pressed button.
  virtual void OnClick(const std::string& notification_id,
                       const std::optional<int>& action_index) = 0;

  // Called when the notification with |notification_id| got closed by either
  // the user, the system or Chrome itself.
  virtual void OnClose(const std::string& notification_id) = 0;
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_NEARBY_NOTIFICATION_DELEGATE_H_
