// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_UI_IDLE_APP_NAME_NOTIFICATION_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_UI_IDLE_APP_NAME_NOTIFICATION_VIEW_H_

#include <memory>

#include "base/macros.h"
#include "base/strings/string16.h"

namespace extensions {
class Extension;
}  // namespace extensions

namespace chromeos {

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
  virtual ~IdleAppNameNotificationView();

  // Close and destroy the message instantly.
  void CloseMessage();

  // Returns true when message is shown.
  bool IsVisible();

  // Returns the shown text for testing.
  base::string16 GetShownTextForTest();

 private:
  // Show the message. This will make the message visible.
  void ShowMessage(int message_visibility_time_in_ms,
                   int animation_time_ms,
                   const extensions::Extension* extension);

  // A reference to an existing message.
  IdleAppNameNotificationDelegateView* view_;

  DISALLOW_COPY_AND_ASSIGN(IdleAppNameNotificationView);
};

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove when moved to chrome/browser/ash/.
namespace ash {
using ::chromeos::IdleAppNameNotificationView;
}

#endif  // CHROME_BROWSER_CHROMEOS_UI_IDLE_APP_NAME_NOTIFICATION_VIEW_H_
