// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_NOTIFICATION_HIDDEN_VIEW_H_
#define ASH_SYSTEM_UNIFIED_NOTIFICATION_HIDDEN_VIEW_H_

#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

namespace ash {

// A view to show the message that notifications are hidden on the lock screen
// by the setting or the flag. This may show the button to encourage the user
// to change the lock screen notification setting if the condition permits.
class NotificationHiddenView : public views::View, views::ButtonListener {
 public:
  NotificationHiddenView();
  ~NotificationHiddenView() override = default;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // views::View:
  const char* GetClassName() const override;

  views::Button* change_button_for_testing() { return change_button_; }

 private:
  views::Button* change_button_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(NotificationHiddenView);
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_NOTIFICATION_HIDDEN_VIEW_H_
