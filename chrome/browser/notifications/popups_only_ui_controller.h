// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_POPUPS_ONLY_UI_CONTROLLER_H_
#define CHROME_BROWSER_NOTIFICATIONS_POPUPS_ONLY_UI_CONTROLLER_H_

#include <memory>

#include "base/macros.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_observer.h"

namespace message_center {
class DesktopMessagePopupCollection;
}  // namespace message_center

// A message center view implementation that shows notification popups (toasts)
// in the corner of the screen, but has no dedicated message center (widget with
// multiple notifications inside). This is used on Windows and Linux for
// non-native notifications.
class PopupsOnlyUiController : public message_center::MessageCenterObserver {
 public:
  PopupsOnlyUiController();
  ~PopupsOnlyUiController() override;

  // MessageCenterObserver:
  void OnNotificationAdded(const std::string& notification_id) override;
  void OnNotificationRemoved(const std::string& notification_id,
                             bool b_user) override;
  void OnNotificationUpdated(const std::string& notification_id) override;
  void OnNotificationClicked(
      const std::string& notification_id,
      const base::Optional<int>& button_index,
      const base::Optional<base::string16>& reply) override;
  void OnBlockingStateChanged(
      message_center::NotificationBlocker* blocker) override;

  bool popups_visible() const { return popups_visible_; }

 private:
  message_center::MessageCenter* const message_center_;
  std::unique_ptr<message_center::DesktopMessagePopupCollection>
      popup_collection_;

  // Update the visibility of the popup bubbles. Shows or hides them if
  // necessary.
  void ShowOrHidePopupBubbles();

  bool popups_visible_ = false;

  DISALLOW_COPY_AND_ASSIGN(PopupsOnlyUiController);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_POPUPS_ONLY_UI_CONTROLLER_H_
