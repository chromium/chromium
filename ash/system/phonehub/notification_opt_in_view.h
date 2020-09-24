// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PHONEHUB_NOTIFICATION_OPT_IN_VIEW_H_
#define ASH_SYSTEM_PHONEHUB_NOTIFICATION_OPT_IN_VIEW_H_

#include "ash/ash_export.h"
#include "ash/system/unified/rounded_label_button.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace views {
class Label;
class LabelButton;
}  // namespace views

namespace ash {

class TrayBubbleView;

// An additional entry point shown on the Phone Hub bubble for the user to grant
// access or opt out for notifications from the phone.
class ASH_EXPORT NotificationOptInView : public views::View,
                                         public views::ButtonListener {
 public:
  METADATA_HEADER(NotificationOptInView);

  explicit NotificationOptInView(TrayBubbleView* bubble_view);
  NotificationOptInView(const NotificationOptInView&) = delete;
  NotificationOptInView& operator=(const NotificationOptInView&) = delete;
  ~NotificationOptInView() override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  views::View* set_up_button_for_testing() { return set_up_button_; }
  views::View* dismiss_button_for_testing() { return dismiss_button_; }

 private:
  void InitLayout();

  // Main components of this view. Owned by view hierarchy.
  views::Label* text_label_ = nullptr;
  RoundedLabelButton* set_up_button_ = nullptr;
  views::LabelButton* dismiss_button_ = nullptr;

  TrayBubbleView* bubble_view_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_PHONEHUB_NOTIFICATION_OPT_IN_VIEW_H_
