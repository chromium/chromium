// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_MESSAGE_CENTER_ARC_NOTIFICATION_MANAGER_DELEGATE_H_
#define ASH_PUBLIC_CPP_MESSAGE_CENTER_ARC_NOTIFICATION_MANAGER_DELEGATE_H_

#include "ash/public/cpp/ash_public_export.h"
#include "base/functional/callback.h"

namespace ash {

// Delegate interface of arc notification code to call into ash code.
class ASH_PUBLIC_EXPORT ArcNotificationManagerDelegate {
 public:
  virtual ~ArcNotificationManagerDelegate() = default;

  // Whether the current user session is public session or kiosk.
  virtual bool IsPublicSessionOrKiosk() const = 0;

  // Shows the message center.
  virtual void ShowMessageCenter() = 0;

  // Hides the message center.
  virtual void HideMessageCenter() = 0;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_MESSAGE_CENTER_ARC_NOTIFICATION_MANAGER_DELEGATE_H_
