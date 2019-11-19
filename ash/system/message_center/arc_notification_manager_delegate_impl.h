// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MESSAGE_CENTER_ARC_NOTIFICATION_MANAGER_DELEGATE_IMPL_H_
#define ASH_SYSTEM_MESSAGE_CENTER_ARC_NOTIFICATION_MANAGER_DELEGATE_IMPL_H_

#include <string>

#include "ash/system/message_center/arc/arc_notification_manager_delegate.h"
#include "base/macros.h"

namespace ash {

class ArcNotificationManagerDelegateImpl
    : public ArcNotificationManagerDelegate {
 public:
  ArcNotificationManagerDelegateImpl();
  ~ArcNotificationManagerDelegateImpl() override;

  // ArcNotificationManagerDelegate:
  bool IsPublicSessionOrKiosk() const override;
  void ShowMessageCenter() override;
  void HideMessageCenter() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ArcNotificationManagerDelegateImpl);
};

}  // namespace ash

#endif  // ASH_SYSTEM_MESSAGE_CENTER_ARC_NOTIFICATION_MANAGER_DELEGATE_IMPL_H_
