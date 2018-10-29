// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_UPDATE_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_UPDATE_SCREEN_H_

#include <set>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/chromeos/login/screens/base_screen.h"
#include "chrome/browser/chromeos/login/screens/error_screen.h"
#include "chromeos/dbus/update_engine_client.h"
#include "chromeos/network/portal_detector/network_portal_detector.h"
#include "components/pairing/host_pairing_controller.h"

namespace chromeos {

class BaseScreenDelegate;
class ErrorScreen;
class ErrorScreensHistogramHelper;
class NetworkState;
class ScreenManager;
class UpdateView;

// Controller for the update screen.
class UpdateScreen : public BaseScreen,
                     public UpdateEngineClient::Observer,
                     public NetworkPortalDetector::Observer {
 public:
  static UpdateScreen* Get(ScreenManager* manager);

  // Returns true if this instance is still active (i.e. has not been deleted).
  static bool HasInstance(UpdateScreen* inst);

  UpdateScreen(BaseScreenDelegate* base_screen_delegate,
               UpdateView* view,
               pairing_chromeos::HostPairingController* remora_controller);
  ~UpdateScreen() override;

  // Called when the being destroyed. This should call Unbind() on the
  // associated View if this class is destroyed before it.
  void OnViewDestroyed(UpdateView* view);

  // Starts network check. Made virtual to simplify mocking.
  virtual void StartNetworkCheck();

  void SetIgnoreIdleStatus(bool ignore_idle_status);

  enum ExitReason {
    REASON_UPDATE_CANCELED = 0,
    REASON_UPDATE_INIT_FAILED,
    REASON_UPDATE_OVER_CELLULAR_REJECTED,
    REASON_UPDATE_NON_CRITICAL,
    REASON_UPDATE_ENDED
  };
  // Reports update results to the BaseScreenDelegate.
  virtual void ExitUpdate(ExitReason reason);

  // UpdateEngineClient::Observer implementation:
  void UpdateStatusChanged(const UpdateEngineClient::Status& status) override;

  // NetworkPortalDetector::Observer implementation:
  void OnPortalDetectionCompleted(
      const NetworkState* network,
      const NetworkPortalDetector::CaptivePortalState& state) override;

  // Skip update UI, usually used only in debug builds/tests.
  void CancelUpdate();

  base::OneShotTimer& GetErrorMessageTimerForTesting();

 private:
  FRIEND_TEST_ALL_PREFIXES(UpdateScreenTest, TestBasic);
  FRIEND_TEST_ALL_PREFIXES(UpdateScreenTest, TestUpdateAvailable);
  FRIEND_TEST_ALL_PREFIXES(UpdateScreenTest, TestAPReselection);
  friend class UpdateScreenUnitTest;

  enum class State {
    STATE_IDLE = 0,
    STATE_FIRST_PORTAL_CHECK,
    STATE_REQUESTING_USER_PERMISSION,
    STATE_UPDATE,
    STATE_ERROR
  };

  // BaseScreen:
  void Show() override;
  void Hide() override;
  void OnUserAction(const std::string& action_id) override;

  // Callback to UpdateEngineClient::SetUpdateOverCellularOneTimePermission
  // called in response to user confirming that the OS update can proceed
  // despite being over cellular charges.
  // |success|: whether the update engine accepted the user permission.
  void RetryUpdateWithUpdateOverCellularPermissionSet(bool success);

  // Updates downloading stats (remaining time and downloading
  // progress) on the AU screen.
  void UpdateDownloadingStats(const UpdateEngineClient::Status& status);

  // Returns true if there is critical system update that requires installation
  // and immediate reboot.
  bool HasCriticalUpdate();

  // Timer notification handlers.
  void OnWaitForRebootTimeElapsed();

  // Checks that screen is shown, shows if not.
  void MakeSureScreenIsShown();

  // Send update status to host pairing controller.
  void SetHostPairingControllerStatus(
      pairing_chromeos::HostPairingController::UpdateStatus update_status);

  // Returns an instance of the error screen.
  ErrorScreen* GetErrorScreen();

  void StartUpdateCheck();
  void ShowErrorMessage();
  void HideErrorMessage();
  void UpdateErrorMessage(
      const NetworkState* network,
      const NetworkPortalDetector::CaptivePortalStatus status);

  void DelayErrorMessage();

  // The user requested an attempt to connect to the network should be made.
  void OnConnectRequested();

  // Timer for the interval to wait for the reboot.
  // If reboot didn't happen - ask user to reboot manually.
  base::OneShotTimer reboot_timer_;

  // Returns a static InstanceSet.
  // TODO(jdufault): There should only ever be one instance of this class.
  // Remove support for supporting multiple instances. See crbug.com/672142.
  typedef std::set<UpdateScreen*> InstanceSet;
  static InstanceSet& GetInstanceSet();

  // Current state of the update screen.
  State state_ = State::STATE_IDLE;

  // Time in seconds after which we decide that the device has not rebooted
  // automatically. If reboot didn't happen during this interval, ask user to
  // reboot device manually.
  int reboot_check_delay_ = 0;

  // True if in the process of checking for update.
  bool is_checking_for_update_ = true;
  // Flag that is used to detect when update download has just started.
  bool is_downloading_update_ = false;
  // If true, update deadlines are ignored.
  // Note, this is false by default.
  bool is_ignore_update_deadlines_ = false;
  // Whether the update screen is shown.
  bool is_shown_ = false;
  // Ignore fist IDLE status that is sent before update screen initiated check.
  bool ignore_idle_status_ = true;

  UpdateView* view_ = nullptr;

  // Used to track updates over Bluetooth.
  pairing_chromeos::HostPairingController* remora_controller_;

  // Time of the first notification from the downloading stage.
  base::Time download_start_time_;
  double download_start_progress_ = 0;

  // Time of the last notification from the downloading stage.
  base::Time download_last_time_;
  double download_last_progress_ = 0;

  bool is_download_average_speed_computed_ = false;
  double download_average_speed_ = 0;

  // True if there was no notification from NetworkPortalDetector
  // about state for the default network.
  bool is_first_detection_notification_ = true;

  // True if there was no notification about captive portal state for
  // the default network.
  bool is_first_portal_notification_ = true;

  // Information about a pending update. Set if a user permission is required to
  // proceed with the update. The values have to be passed to the update engine
  // in SetUpdateOverCellularOneTimePermission method in order to enable update
  // over cellular network.
  std::string pending_update_version_;
  int64_t pending_update_size_ = 0;

  std::unique_ptr<ErrorScreensHistogramHelper> histogram_helper_;

  // Timer for the captive portal detector to show portal login page.
  // If redirect did not happen during this delay, error message is shown
  // instead.
  base::OneShotTimer error_message_timer_;

  ErrorScreen::ConnectRequestCallbackSubscription connect_request_subscription_;

  base::WeakPtrFactory<UpdateScreen> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(UpdateScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_UPDATE_SCREEN_H_
