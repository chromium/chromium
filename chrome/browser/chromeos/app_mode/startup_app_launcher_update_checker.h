// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_APP_MODE_STARTUP_APP_LAUNCHER_UPDATE_CHECKER_H_
#define CHROME_BROWSER_CHROMEOS_APP_MODE_STARTUP_APP_LAUNCHER_UPDATE_CHECKER_H_

#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class Profile;

namespace ash {

// Used by StartupAppLauncher to check for available extension updates for
// extensions other than the primary kiosk app - in particular for the secondary
// extensions and imports defined by the primary app.
class StartupAppLauncherUpdateChecker : public content::NotificationObserver {
 public:
  explicit StartupAppLauncherUpdateChecker(Profile* profile);
  StartupAppLauncherUpdateChecker(const StartupAppLauncherUpdateChecker&) =
      delete;
  StartupAppLauncherUpdateChecker& operator=(
      const StartupAppLauncherUpdateChecker&) = delete;
  ~StartupAppLauncherUpdateChecker() override;

  using UpdateCheckCallback = base::OnceCallback<void(bool updates_found)>;
  // Runs the extension update check.
  // |callback| is called when the update check completes, with a boolean value
  // indicating whether an update for an extension was found.
  // Returns whether the update check has successfully started - callback will
  // eventually be run only if the return value is true.
  // It is safe to delete |this| before the update check is done - in that case
  // the callback will never run.
  bool Run(UpdateCheckCallback callback);

  // content::NotificationObserver:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

 private:
  // Callback for extension updater check.
  void OnExtensionUpdaterDone();

  const raw_ptr<Profile> profile_;

  // Whether an extensions with an available update has been detected.
  bool update_found_ = false;

  UpdateCheckCallback callback_;

  content::NotificationRegistrar registrar_;

  base::WeakPtrFactory<StartupAppLauncherUpdateChecker> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_CHROMEOS_APP_MODE_STARTUP_APP_LAUNCHER_UPDATE_CHECKER_H_
