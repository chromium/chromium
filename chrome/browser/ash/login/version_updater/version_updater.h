// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_VERSION_UPDATER_VERSION_UPDATER_H_
#define CHROME_BROWSER_ASH_LOGIN_VERSION_UPDATER_VERSION_UPDATER_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/login/screens/network_error.h"
#include "chrome/browser/ash/login/version_updater/update_time_estimator.h"
#include "chromeos/ash/components/dbus/update_engine/update_engine_client.h"
#include "chromeos/ash/components/network/network_state_handler_observer.h"

namespace base {
class DefaultTickClock;
}

namespace ash {

class NetworkState;

// Tries to update system, interacting with UpdateEnglineClient and
// NetworkPortalDetector. Uses callbacks - methods of `delegate_`, which may
// interact with user, change UI etc.
class VersionUpdater : public UpdateEngineClient::Observer,
                       public NetworkStateHandlerObserver {
 public:
  enum class Result {
    UPDATE_NOT_REQUIRED,
    UPDATE_ERROR,
    UPDATE_SKIPPED,
    UPDATE_OPT_OUT_INFO_SHOWN,
    UPDATE_CHECK_TIMEOUT,
  };

  enum class State {
    STATE_IDLE = 0,
    STATE_REQUESTING_USER_PERMISSION,
    STATE_UPDATE,
    STATE_ERROR
  };

  // Stores information about current downloading process, update progress and
  // state.
  struct UpdateInfo {
    UpdateInfo();

    update_engine::StatusResult status;

    // Time left for an update to finish in seconds.
    base::TimeDelta total_time_left;
    // Progress of an update which is based on total time left expectation.
    int better_update_progress = 0;

    // Estimated time left for only downloading stage, in seconds.
    // TODO(crbug.com/40703499): Remove when better update is launched.
    int estimated_time_left_in_secs = 0;
    bool show_estimated_time_left = false;

    // True if VersionUpdater in such a state that progress is not available or
    // applicable (e.g. checking for updates)
    bool progress_unavailable = true;
    std::u16string progress_message = std::u16string();
    // Percent of update progress, between 0 and 100.
    int progress = 0;

    bool requires_permission_for_cellular = false;

    // Information about a pending update. Set if a user permission is required
    // to proceed with the update. The values have to be passed to the update
    // engine in SetUpdateOverCellularOneTimePermission method in order to
    // enable update over cellular network.
    int64_t update_size = 0;
    std::string update_version = std::string();

    // True if in the process of checking for update.
    bool is_checking_for_update = true;

    // Current state.
    State state = State::STATE_IDLE;
  };

  // Interface for callbacks that are called when corresponding events occur
  // during update process.
  class Delegate {
   public:
    // Called when update info changes
    virtual void UpdateInfoChanged(
        const VersionUpdater::UpdateInfo& update_info) = 0;
    // Reports update results.
    virtual void FinishExitUpdate(VersionUpdater::Result result) = 0;
    // Timer notification handler.
    virtual void OnWaitForRebootTimeElapsed() = 0;
    // Called before update check starts.
    virtual void PrepareForUpdateCheck() = 0;
    virtual void UpdateErrorMessage(NetworkState::PortalState state,
                                    NetworkError::ErrorState error_state,
                                    const std::string& network_name) = 0;
    virtual void ShowErrorMessage() = 0;
    virtual void DelayErrorMessage() = 0;
  };

  // Callback type for `GetEOLInfo`
  using EolInfoCallback =
      base::OnceCallback<void(const UpdateEngineClient::EolInfo& eol_info)>;

  explicit VersionUpdater(VersionUpdater::Delegate* delegate);

  VersionUpdater(const VersionUpdater&) = delete;
  VersionUpdater& operator=(const VersionUpdater&) = delete;

  ~VersionUpdater() override;

  // Resets `VersionUpdater` to initial state.
  void Init();

  // Starts network check. If success, starts update check.
  void StartNetworkCheck();
  void StartUpdateCheck();

  // Cleans up observer registrations for this object.
  void StopObserving();

  void RefreshTimeLeftEstimation();

  void SetUpdateOverCellularOneTimePermission();
  void RejectUpdateOverCellular();
  void RebootAfterUpdate();
  void StartExitUpdate(Result result);

  const UpdateInfo& update_info() { return update_info_; }

  // Has the device already reached its End Of Life (Auto Update Expiration) ?
  void GetEolInfo(EolInfoCallback callback);

  void set_tick_clock_for_testing(const base::TickClock* tick_clock) {
    tick_clock_ = tick_clock;
    time_estimator_.set_tick_clock_for_testing(tick_clock);
  }

  void set_wait_for_reboot_time_for_testing(
      base::TimeDelta wait_for_reboot_time) {
    wait_for_reboot_time_ = wait_for_reboot_time;
  }

  base::OneShotTimer* get_retry_check_timer_for_testing() {
    return &retry_check_timer_;
  }

  bool get_non_idle_status_received_for_testing() {
    return non_idle_status_received_;
  }

  base::OneShotTimer* get_reboot_timer_for_testing() { return &reboot_timer_; }

  void UpdateStatusChangedForTesting(const update_engine::StatusResult& status);

 private:
  void RequestUpdateCheck();
  void TriggerUpdateCheck();
  void OnRetryCheckElapsed();

  void OnGetEolInfo(EolInfoCallback cb, UpdateEngineClient::EolInfo info);

  // UpdateEngineClient::Observer implementation:
  void UpdateStatusChanged(const update_engine::StatusResult& status) override;

  // NetworkStateHandlerObserver implementation:
  void PortalStateChanged(
      const NetworkState* default_network,
      const NetworkState::PortalState portal_state) override;
  void OnShuttingDown() override;

  void OnWaitForRebootTimeElapsed();

  void UpdateErrorMessage(const NetworkState* network,
                          NetworkState::PortalState state);

  // Callback to UpdateEngineClient::SetUpdateOverCellularOneTimePermission
  // called in response to user confirming that the OS update can proceed
  // despite being over cellular charges.
  // `success`: whether the update engine accepted the user permission.
  void OnSetUpdateOverCellularOneTimePermission(bool success);

  // Callback for UpdateEngineClient::RequestUpdateCheck() called from
  // StartUpdateCheck().
  void OnUpdateCheckStarted(UpdateEngineClient::UpdateCheckResult result);

  // Pointer to delegate that owns this VersionUpdater instance.
  raw_ptr<Delegate> delegate_;

  std::unique_ptr<base::RepeatingTimer> refresh_timer_;

  // Timer for the interval to wait for the reboot.
  // If reboot didn't happen - ask user to reboot manually.
  base::OneShotTimer reboot_timer_;
  // Time in seconds after which we decide that the device has not rebooted
  // automatically. If reboot didn't happen during this interval, ask user to
  // reboot device manually.
  base::TimeDelta wait_for_reboot_time_;

  // True once we have received a non-IDLE status. If we first receive an IDLE
  // status, then we are getting a signal from a previous request which may have
  // been in-progress when our update was sent, and we should resend the update.
  // Once we have received a non-IDLE, then IDLE means we can exit.
  bool non_idle_status_received_ = false;

  // Timer for the interval to wait trying reaching to the update screen before
  // exiting the screen.
  base::OneShotTimer retry_check_timer_;

  // Time to retry reaching to update_engine before exit.
  base::TimeDelta retry_check_timeout_ = base::Seconds(180);

  // Current count of retiries to request `checking of update`.
  int num_retries_ = 0;

  base::TimeTicks checking_for_update_start_;

  // Stores information about current downloading process, update progress and
  // state. It is sent to Delegate on each UpdateInfoChanged call, and also can
  // be obtained with corresponding getter.
  UpdateInfo update_info_;

  UpdateTimeEstimator time_estimator_;

  raw_ptr<const base::TickClock> tick_clock_;

  base::WeakPtrFactory<VersionUpdater> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_VERSION_UPDATER_VERSION_UPDATER_H_
