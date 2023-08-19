// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NOTIFICATIONS_UPDATE_NOTIFICATION_SHOWING_CONTROLLER_H_
#define CHROME_BROWSER_ASH_NOTIFICATIONS_UPDATE_NOTIFICATION_SHOWING_CONTROLLER_H_

#include <memory>

#include "base/memory/raw_ptr.h"

class Profile;
class PrefRegistrySimple;

namespace ash {

class UpdateNotification;

// The controller to show the `UpdateNotification`.
class UpdateNotificationShowingController {
 public:
  // Registers profile prefs.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  explicit UpdateNotificationShowingController(Profile* profile);
  UpdateNotificationShowingController(
      const UpdateNotificationShowingController&) = delete;
  UpdateNotificationShowingController& operator=(
      const UpdateNotificationShowingController&) = delete;
  ~UpdateNotificationShowingController();

  // Determines if the `UpdateNotification` should be shown to the user and
  // shows it if so. This will not do anything if a `UpdateNotification` has
  // already been shown in the current milestone.
  void MaybeShowUpdateNotification();

  // Updates user preference and logs UMA tracking.
  void MarkNotificationShown();

 private:
  friend class UpdateNotificationTest;
  friend class UpdateNotificationShowingControllerTest;

  // Sets a fake milestone as the current milestone. Only used in tests.
  void SetFakeCurrentMilestoneForTesting(int fake_milestone);

  int current_milestone_;
  const raw_ptr<Profile, ExperimentalAsh> profile_;
  std::unique_ptr<UpdateNotification> update_notification_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_NOTIFICATIONS_UPDATE_NOTIFICATION_SHOWING_CONTROLLER_H_
