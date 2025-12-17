// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_RESTORE_BROWSER_RESTORE_OBSERVER_H_
#define CHROME_BROWSER_ASH_APP_RESTORE_BROWSER_RESTORE_OBSERVER_H_

#include "base/callback_list.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/browser_delegate/browser_controller.h"

class Profile;

namespace ash {

class BrowserDelegate;

// An observer that restores urls based on the on startup setting after a new
// browser is added to the BrowserList.
class BrowserRestoreObserver : public BrowserController::Observer {
 public:
  BrowserRestoreObserver();
  BrowserRestoreObserver(const BrowserRestoreObserver&) = delete;
  BrowserRestoreObserver& operator=(const BrowserRestoreObserver&) = delete;
  ~BrowserRestoreObserver() override;

  // BrowserController::Observer:
  void OnBrowserCreated(BrowserDelegate* browser) override;

  static bool CanRestoreUrlsForProfileForTesting(const Profile* profile);

 private:
  // Called when a session is restored.
  void OnSessionRestoreDone(Profile* profile, int num_tabs_restored);

  // Restores urls based on the on startup setting.
  void RestoreUrls(BrowserDelegate* browser);

  base::ScopedObservation<BrowserController, BrowserController::Observer>
      browser_controller_observation_{this};
  base::CallbackListSubscription on_session_restored_callback_subscription_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_APP_RESTORE_BROWSER_RESTORE_OBSERVER_H_
