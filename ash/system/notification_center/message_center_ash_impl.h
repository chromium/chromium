// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NOTIFICATION_CENTER_MESSAGE_CENTER_ASH_IMPL_H_
#define ASH_SYSTEM_NOTIFICATION_CENTER_MESSAGE_CENTER_ASH_IMPL_H_

#include "ash/public/cpp/message_center_ash.h"
#include "ui/message_center/message_center_observer.h"

namespace ash {

class MessageCenterAshImpl : public MessageCenterAsh,
                             public message_center::MessageCenterObserver {
 public:
  MessageCenterAshImpl();
  ~MessageCenterAshImpl() override;

 private:
  // MessageCenterAsh override:
  void SetQuietMode(bool in_quiet_mode) override;
  bool IsQuietMode() const override;

  // MessageCenterObserver override:
  void OnQuietModeChanged(bool in_quiet_mode) override;
};

}  // namespace ash

#endif  // ASH_SYSTEM_NOTIFICATION_CENTER_MESSAGE_CENTER_ASH_IMPL_H_