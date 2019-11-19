// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_ARC_NOTIFICATIONS_HOST_INITIALIZER_H_
#define ASH_PUBLIC_CPP_ARC_NOTIFICATIONS_HOST_INITIALIZER_H_

#include "ash/public/cpp/ash_public_export.h"
#include "components/arc/mojom/notifications.mojom.h"

namespace ash {

class ASH_PUBLIC_EXPORT ArcNotificationsHostInitializer {
 public:
  static ArcNotificationsHostInitializer* Get();

  virtual void SetArcNotificationsInstance(
      arc::mojom::NotificationsInstancePtr arc_notification_instance) = 0;

 protected:
  ArcNotificationsHostInitializer();
  virtual ~ArcNotificationsHostInitializer();
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_ARC_NOTIFICATIONS_HOST_INITIALIZER_H_
