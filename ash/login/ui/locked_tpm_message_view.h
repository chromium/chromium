// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_LOCKED_TPM_MESSAGE_VIEW_H_
#define ASH_LOGIN_UI_LOCKED_TPM_MESSAGE_VIEW_H_

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "ui/gfx/canvas.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/view.h"

namespace ash {

// The message that can be shown to the user when TPM is locked.
class LockedTpmMessageView : public views::View {
 public:
  LockedTpmMessageView();
  LockedTpmMessageView(const LockedTpmMessageView&) = delete;
  LockedTpmMessageView& operator=(const LockedTpmMessageView&) = delete;
  ~LockedTpmMessageView() override;

  // Set remaining time to be shown in the message.
  void SetRemainingTime(base::TimeDelta time_left);

  // views::View:
  void RequestFocus() override;

 private:
  views::Label* CreateLabel();

  base::TimeDelta prev_time_left_;
  raw_ptr<views::Label> message_warning_;
  raw_ptr<views::Label> message_description_;
  raw_ptr<views::ImageView> message_icon_;
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_LOCKED_TPM_MESSAGE_VIEW_H_
