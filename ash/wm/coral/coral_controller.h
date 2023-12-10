// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_CORAL_CORAL_CONTROLLER_H_
#define ASH_WM_CORAL_CORAL_CONTROLLER_H_

#include "ash/ash_export.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

namespace ash {

class ASH_EXPORT CoralController : public message_center::NotificationObserver {
 public:
  CoralController();
  CoralController(const CoralController&) = delete;
  CoralController& operator=(const CoralController&) = delete;
  virtual ~CoralController();

  static const char kDataCollectionNotificationId[];

  // Returns true if the provided secret key is the correct secret key. UI will
  // only be shown when it's matched.
  static bool IsSecretKeyMatched();

  // message_center::NotificationObserver:
  void Click(const std::optional<int>& button_index,
             const std::optional<std::u16string>& reply) override;

 private:
  void CollectDataPeriodically();

  // The repeating timer to collect data.
  base::RepeatingTimer data_collection_timer_;

  base::WeakPtrFactory<CoralController> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_WM_CORAL_CORAL_CONTROLLER_H_
