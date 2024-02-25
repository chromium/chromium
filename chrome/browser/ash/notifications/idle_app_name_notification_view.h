// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NOTIFICATIONS_IDLE_APP_NAME_NOTIFICATION_VIEW_H_
#define CHROME_BROWSER_ASH_NOTIFICATIONS_IDLE_APP_NAME_NOTIFICATION_VIEW_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"

namespace extensions {
class Extension;
}  // namespace extensions

namespace ash {

class IdleAppNameNotificationDelegateView;

// A class which creates a message which shows the currently running applicaion
// name and its creator.
class IdleAppNameNotificationView {
 public:
  // |message_visibility_time_in_ms| is the time the message is fully visible.
  // |animation_time_ms| is the transition time for the message to show or hide.
  // |extension| is the application which is started.
  IdleAppNameNotificationView(int message_visibility_time_in_ms,
                              int animation_time_ms,
                              const extensions::Extension* extension);

  IdleAppNameNotificationView(const IdleAppNameNotificationView&) = delete;
  IdleAppNameNotificationView& operator=(const IdleAppNameNotificationView&) =
      delete;

  virtual ~IdleAppNameNotificationView();

  // Close and destroy the message instantly.
  void CloseMessage();

  // Returns true when message is shown.
  bool IsVisible();

  // Returns the shown text for testing.
  std::u16string GetShownTextForTest();

 private:
  // Show the message. This will make the message visible.
  void ShowMessage(int message_visibility_time_in_ms,
                   int animation_time_ms,
                   const extensions::Extension* extension);

  // A reference to an existing message.
  raw_ptr<IdleAppNameNotificationDelegateView> view_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_NOTIFICATIONS_IDLE_APP_NAME_NOTIFICATION_VIEW_H_
