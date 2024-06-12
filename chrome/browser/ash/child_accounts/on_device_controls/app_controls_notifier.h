// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CHILD_ACCOUNTS_ON_DEVICE_CONTROLS_APP_CONTROLS_NOTIFIER_H_
#define CHROME_BROWSER_ASH_CHILD_ACCOUNTS_ON_DEVICE_CONTROLS_APP_CONTROLS_NOTIFIER_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"

class PrefRegistrySimple;
class Profile;

namespace ash::on_device_controls {

// Displays and manages a notification informing eligible users that on-device
// app controls are available.
class AppControlsNotifier {
 public:
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  explicit AppControlsNotifier(Profile* profile);
  AppControlsNotifier(const AppControlsNotifier&) = delete;
  AppControlsNotifier& operator=(const AppControlsNotifier&) = delete;
  ~AppControlsNotifier();

  // Triggers a notification that app controls are available if the user is
  // eligible and has not yet been shown the notification.
  void MaybeShowAppControlsNotification();

 private:
  friend class AppControlsNotifierTest;

  void HandleClick(std::optional<int> button_index);

  void OpenAppsSettings();

  bool ShouldShowNotification() const;

  void ShowNotification();

  const raw_ptr<Profile> profile_;

  base::WeakPtrFactory<AppControlsNotifier> weak_ptr_factory_{this};
};

}  // namespace ash::on_device_controls

#endif  // CHROME_BROWSER_ASH_CHILD_ACCOUNTS_ON_DEVICE_CONTROLS_APP_CONTROLS_NOTIFIER_H_
