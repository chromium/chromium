// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FIRST_RUN_DRIVE_FIRST_RUN_CONTROLLER_H_
#define CHROME_BROWSER_ASH_FIRST_RUN_DRIVE_FIRST_RUN_CONTROLLER_H_

#include "base/observer_list.h"
#include "base/timer/timer.h"
#include "chrome/browser/profiles/profile.h"

namespace ash {

class DriveWebContentsManager;

// This class is responsible for kicking off the Google Drive offline
// initialization process. There is an initial delay to avoid contention when
// the session starts. DriveFirstRunController will manage its own lifetime and
// destroy itself when the initialization succeeds or fails.
class DriveFirstRunController {
 public:
  // This enum is used for UMA metrics. Keep the fields in the same order as
  // the "CrosEnableDriveOfflineOutcome" enum in histograms.xml.
  enum UMAOutcome {
    OUTCOME_OFFLINE_ENABLED,
    OUTCOME_WEB_CONTENTS_TIMED_OUT,
    OUTCOME_WEB_CONTENTS_LOAD_FAILED,
    OUTCOME_WRONG_USER_TYPE,
    OUTCOME_APP_NOT_INSTALLED,
    OUTCOME_BACKGROUND_PAGE_EXISTS,
    OUTCOME_MAX,
  };

  class Observer {
   public:
    // Called when enabling offline mode times out. OnCompletion will be called
    // immediately afterwards.
    virtual void OnTimedOut() = 0;

    // Called when the first run flow finishes, informing the observer of
    // success or failure.
    virtual void OnCompletion(bool success) = 0;

   protected:
    virtual ~Observer() {}
  };

  explicit DriveFirstRunController(Profile* profile);

  DriveFirstRunController(const DriveFirstRunController&) = delete;
  DriveFirstRunController& operator=(const DriveFirstRunController&) = delete;

  ~DriveFirstRunController();

  // Starts the process to enable offline mode for the user's Drive account.
  void EnableOfflineMode();

  // Manages observers of the first run flow.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Set delay times for testing purposes.
  void SetDelaysForTest(int initial_delay_secs, int timeout_secs);

  // Set the app id and endpoint url for testing purposes.
  void SetAppInfoForTest(const std::string& app_id,
                         const std::string& endpoint_url);

 private:
  // Used as a callback to indicate whether the offline initialization
  // succeeds or fails.
  void OnOfflineInit(bool success, UMAOutcome outcome);

  // Called when timed out waiting for offline initialization to complete.
  void OnWebContentsTimedOut();

  // Creates and shows a system notification when enable offline succeeds.
  void ShowNotification();

  // Cleans up internal state and schedules self for deletion.
  void CleanUp();

  Profile* profile_;
  std::unique_ptr<DriveWebContentsManager> web_contents_manager_;
  base::OneShotTimer web_contents_timer_;
  base::OneShotTimer initial_delay_timer_;
  bool started_;
  base::ObserverList<Observer>::Unchecked observer_list_;

  int initial_delay_secs_;
  int web_contents_timeout_secs_;
  std::string drive_offline_endpoint_url_;
  std::string drive_hosted_app_id_;
};

}  // namespace ash

// TODO(https://crbug.com/1164001): remove when ChromeOS code migration is done.
namespace chromeos {
using ::ash::DriveFirstRunController;
}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace ash {
using ::chromeos::DriveFirstRunController;
}

#endif  // CHROME_BROWSER_ASH_FIRST_RUN_DRIVE_FIRST_RUN_CONTROLLER_H_
