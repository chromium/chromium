// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NOTIFICATION_CENTER_ARC_NOTIFICATION_MANAGER_DELEGATE_IMPL_H_
#define ASH_SYSTEM_NOTIFICATION_CENTER_ARC_NOTIFICATION_MANAGER_DELEGATE_IMPL_H_

#include "ash/public/cpp/message_center/arc_notification_manager_delegate.h"

namespace ash {

class ArcNotificationManagerDelegateImpl
    : public ArcNotificationManagerDelegate {
 public:
  ArcNotificationManagerDelegateImpl();

  ArcNotificationManagerDelegateImpl(
      const ArcNotificationManagerDelegateImpl&) = delete;
  ArcNotificationManagerDelegateImpl& operator=(
      const ArcNotificationManagerDelegateImpl&) = delete;

  ~ArcNotificationManagerDelegateImpl() override;

  // ArcNotificationManagerDelegate:
  bool IsManagedGuestSessionOrKiosk() const override;
  void ShowMessageCenter() override;
  void HideMessageCenter() override;
};

}  // namespace ash

#endif  // ASH_SYSTEM_NOTIFICATION_CENTER_ARC_NOTIFICATION_MANAGER_DELEGATE_IMPL_H_
