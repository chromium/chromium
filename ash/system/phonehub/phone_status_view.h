// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PHONEHUB_PHONE_STATUS_VIEW_H_
#define ASH_SYSTEM_PHONEHUB_PHONE_STATUS_VIEW_H_

#include "ash/ash_export.h"
#include "ash/system/tray/tri_view.h"
#include "ash/system/unified/top_shortcut_button.h"
#include "ui/views/controls/button/button.h"

namespace views {
class ImageView;
}

namespace ash {

// The header row at the top of the Phone Hub panel, showing phone title and
// status (wifi, volime, etc.).
class ASH_EXPORT PhoneStatusView : public TriView,
                                   public views::ButtonListener {
 public:
  PhoneStatusView();
  ~PhoneStatusView() override;
  PhoneStatusView(PhoneStatusView&) = delete;
  PhoneStatusView operator=(PhoneStatusView&) = delete;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

 private:
  void ConfigureTriViewContainer(TriView::Container container);

  views::ImageView* wifi_icon_ = nullptr;
  views::ImageView* volume_icon_ = nullptr;
  views::ImageView* battery_icon_ = nullptr;
  TopShortcutButton* settings_button_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_PHONEHUB_PHONE_STATUS_VIEW_H_
