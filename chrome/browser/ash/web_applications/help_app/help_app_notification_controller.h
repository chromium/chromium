// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_WEB_APPLICATIONS_HELP_APP_HELP_APP_NOTIFICATION_CONTROLLER_H_
#define CHROME_BROWSER_ASH_WEB_APPLICATIONS_HELP_APP_HELP_APP_NOTIFICATION_CONTROLLER_H_

#include <memory>

#include "base/memory/weak_ptr.h"

class Profile;
class PrefRegistrySimple;

namespace ash {
class ReleaseNotesNotification;
}  // namespace ash

namespace chromeos {

class HelpAppDiscoverTabNotification;

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

  // Determines which notification to show to the user, if any at all. This will
  // never show more than 1 notification, to avoid spamming the user.
  void MaybeShowNotification();

  // Determines if the discover notification should be shown to the user and
  // shows it if so. Will produce an additional notification on top of
  // |MaybeShowNotification|.
  void MaybeShowDiscoverNotification();

 private:
  Profile* const profile_;
  std::unique_ptr<HelpAppDiscoverTabNotification> discover_tab_notification_;
  std::unique_ptr<ash::ReleaseNotesNotification> release_notes_notification_;

  base::WeakPtrFactory<HelpAppNotificationController> weak_ptr_factory_{this};
};

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace ash {
using ::chromeos::HelpAppNotificationController;
}

#endif  // CHROME_BROWSER_ASH_WEB_APPLICATIONS_HELP_APP_HELP_APP_NOTIFICATION_CONTROLLER_H_
