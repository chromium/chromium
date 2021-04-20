// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYNC_TURN_SYNC_ON_HELPER_H_
#define CHROME_BROWSER_ASH_SYNC_TURN_SYNC_ON_HELPER_H_

#include <memory>

#include "base/scoped_observation.h"
#include "chrome/browser/sync/sync_startup_tracker.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"

class Browser;
class PrefRegistrySimple;
class Profile;

namespace signin {
class IdentityManager;
}  // namespace signin

namespace syncer {
class SyncService;
}  // namespace syncer

// Shows the browser sync consent dialog and turns on sync if the user consents.
// Similar to DiceTurnSyncOnHelper, but Chrome OS doesn't use DICE and doesn't
// allow the user to sign out.
// TODO(crbug.com/1036440): For development purposes we show the dialog
// immediately when the first browser window opens. Long-term the browser will
// open a page similar to chrome://welcome on first run. Once this browser
// first-run flow is implemented the BrowserListObserver can be removed.
class TurnSyncOnHelper : public SyncStartupTracker::Observer,
                         public LoginUIService::Observer,
                         public BrowserListObserver {
 public:
  // Delegate to stub out the UI for testing.
  class Delegate {
   public:
    virtual ~Delegate() = default;
    virtual void ShowSyncConfirmation(Profile* profile, Browser* browser) = 0;
    virtual void ShowSyncSettings(Profile* profile, Browser* browser) = 0;
  };
  // Uses the production delegate with real UI.
  explicit TurnSyncOnHelper(Profile* profile);
  // Exposed for testing.
  TurnSyncOnHelper(Profile* profile, std::unique_ptr<Delegate> delegate);
  ~TurnSyncOnHelper() override;

  TurnSyncOnHelper(const TurnSyncOnHelper&) = delete;
  TurnSyncOnHelper& operator=(const TurnSyncOnHelper&) = delete;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // BrowserListObserver:
  void OnBrowserSetLastActive(Browser* browser) override;

  // SyncStartupTracker::Observer:
  void SyncStartupCompleted() override;
  void SyncStartupFailed() override;

  // LoginUIService::Observer:
  void OnSyncConfirmationUIClosed(
      LoginUIService::SyncConfirmationUIClosedResult result) override;

 private:
  // Starts observing for new browser windows if needed.
  void Init();

  // Starts the setup flow.
  void StartFlow();

  // Displays the Sync confirmation UI.
  // Note: If sync fails to start (e.g. sync is disabled by admin), the sync
  // confirmation dialog will be updated accordingly.
  void ShowSyncConfirmationUI();

  // Handles the user input from the sync confirmation UI.
  void FinishSyncSetup(LoginUIService::SyncConfirmationUIClosedResult result);

  // Returns the SyncService, or nullptr if sync is not allowed.
  syncer::SyncService* GetSyncService();

  Profile* const profile_;
  signin::IdentityManager* const identity_manager_;
  std::unique_ptr<Delegate> delegate_;
  Browser* browser_ = nullptr;
  std::unique_ptr<SyncStartupTracker> sync_startup_tracker_;

  base::ScopedObservation<LoginUIService, LoginUIService::Observer>
      scoped_login_ui_service_observation_{this};
};

#endif  // CHROME_BROWSER_ASH_SYNC_TURN_SYNC_ON_HELPER_H_
