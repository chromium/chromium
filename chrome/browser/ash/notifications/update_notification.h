// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NOTIFICATIONS_UPDATE_NOTIFICATION_H_
#define CHROME_BROWSER_ASH_NOTIFICATIONS_UPDATE_NOTIFICATION_H_

#include "base/memory/weak_ptr.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class Profile;

namespace ash {

class UpdateNotificationShowingController;

// The notification to show an update message.
class UpdateNotification {
 public:
  UpdateNotification(Profile* profile,
                     UpdateNotificationShowingController* controller);
  UpdateNotification(const UpdateNotification&) = delete;
  UpdateNotification& operator=(const UpdateNotification&) = delete;
  ~UpdateNotification();

  // Shows the notification.
  void ShowNotification();

 private:
  // Handles clicks on the notification.
  void OnNotificationClick(absl::optional<int> button_index);

  const raw_ptr<Profile, DanglingUntriaged | ExperimentalAsh> profile_;
  const raw_ptr<UpdateNotificationShowingController, ExperimentalAsh>
      controller_;

  base::WeakPtrFactory<UpdateNotification> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_NOTIFICATIONS_UPDATE_NOTIFICATION_H_
