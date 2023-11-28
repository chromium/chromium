// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_POWER_ADAPTIVE_CHARGING_NOTIFICATION_CONTROLLER_H_
#define ASH_SYSTEM_POWER_ADAPTIVE_CHARGING_NOTIFICATION_CONTROLLER_H_

#include <optional>
#include <string>

#include "ash/ash_export.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

namespace ash {

class ASH_EXPORT AdaptiveChargingNotificationController
    : public message_center::NotificationObserver {
 public:
  AdaptiveChargingNotificationController();
  AdaptiveChargingNotificationController(
      const AdaptiveChargingNotificationController&) = delete;
  AdaptiveChargingNotificationController& operator=(
      const AdaptiveChargingNotificationController&) = delete;
  virtual ~AdaptiveChargingNotificationController();

  // Show the adaptive charging notification. There are two possible
  // scenarios:
  // 1. When the battery is kept at 80% indefinitely (i.e. no value of
  // |time_to_full| is provided), the notification is a
  // normal notification.
  // 2. When the battery is temporary kept at 80% (i.e. |time_to_full| has
  // a value), the estimated time to full will be shown.
  void ShowAdaptiveChargingNotification(
      std::optional<base::TimeDelta> time_to_full = std::nullopt);

  void CloseAdaptiveChargingNotification(bool by_user = false);

  bool ShouldShowNotification();

  // message_center::NotificationObserver:
  void Click(const std::optional<int>& button_index,
             const std::optional<std::u16string>& reply) override;

 private:
  base::WeakPtrFactory<AdaptiveChargingNotificationController>
      weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_POWER_ADAPTIVE_CHARGING_NOTIFICATION_CONTROLLER_H_
