// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_HELP_APP_HELP_APP_NOTIFICATION_CONTROLLER_H_
#define CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_HELP_APP_HELP_APP_NOTIFICATION_CONTROLLER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"

class Profile;
class PrefRegistrySimple;

namespace ash {

class ReleaseNotesNotification;

// Class to show notifications under the Help App.
class HelpAppNotificationController {
 public:
  // Registers profile prefs.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  explicit HelpAppNotificationController(Profile* profile);
  HelpAppNotificationController(const HelpAppNotificationController&) = delete;
  HelpAppNotificationController& operator=(
      const HelpAppNotificationController&) = delete;
  ~HelpAppNotificationController();

  // Determines if the Release Notes notification should be shown to the user
  // and shows it if so. This will not do anything if a Help app notification
  // has already been shown in the current milestone.
  void MaybeShowReleaseNotesNotification();

 private:
  const raw_ptr<Profile> profile_;
  std::unique_ptr<ReleaseNotesNotification> release_notes_notification_;

  base::WeakPtrFactory<HelpAppNotificationController> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_HELP_APP_HELP_APP_NOTIFICATION_CONTROLLER_H_
