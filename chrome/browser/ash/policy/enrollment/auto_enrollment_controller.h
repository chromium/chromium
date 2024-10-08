// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_ENROLLMENT_AUTO_ENROLLMENT_CONTROLLER_H_
#define CHROME_BROWSER_ASH_POLICY_ENROLLMENT_AUTO_ENROLLMENT_CONTROLLER_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/policy/enrollment/auto_enrollment_client.h"
#include "chrome/browser/ash/policy/enrollment/auto_enrollment_state.h"
#include "chrome/browser/ash/policy/enrollment/auto_enrollment_type_checker.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_state_fetcher.h"
#include "chrome/browser/ash/policy/enrollment/psm/rlwe_dmserver_client_impl.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chromeos/ash/components/dbus/cryptohome/UserDataAuth.pb.h"
#include "chromeos/ash/components/dbus/device_management/device_management_interface.pb.h"
#include "chromeos/ash/components/network/network_state_handler_observer.h"

namespace ash {
class InstallAttributesClient;
class NetworkStateHandler;
class SystemClockSyncObservation;
}  // namespace ash

namespace policy {

class DeviceManagementService;
class ServerBackedStateKeysBroker;

// Helper class to obtain FWMP flags.
// See b/268267865.
class EnrollmentFwmpHelper {
 public:
  using ResultCallback = base::OnceCallback<void(bool)>;

  // `install_attributes_client` has to be not nullptr. It will be used to
  // obtain the FWMP flags.
  explicit EnrollmentFwmpHelper(
      ash::InstallAttributesClient* install_attributes_client);
  EnrollmentFwmpHelper(const EnrollmentFwmpHelper&) = delete;
  EnrollmentFwmpHelper& operator=(const EnrollmentFwmpHelper&) = delete;
  ~EnrollmentFwmpHelper();

  // Read FWMP.dev_disable_boot (a.k.a. block_devmode) and return the
  // value asynchronously via result_callback.
  // Return `false` in case of errors (e.g. `install_attributes_client_` or
  // FWMP not available).
  void DetermineDevDisableBoot(ResultCallback result_callback);

 private:
  void RequestFirmwareManagementParameters(ResultCallback result_callback,
                                           bool service_is_ready);

  void OnGetFirmwareManagementParametersReceived(
      ResultCallback result_callback,
      std::optional<device_management::GetFirmwareManagementParametersReply>
          reply);

  raw_ptr<ash::InstallAttributesClient> install_attributes_client_;
  base::WeakPtrFactory<EnrollmentFwmpHelper> weak_ptr_factory_{this};
};

// Drives the forced re-enrollment check (for historical reasons called
// auto-enrollment check), running an `AutoEnrollmentClient` if appropriate to
// make a decision.
// The controller tracks network status to retry when the device is going
// online in case of a prior failure.
class AutoEnrollmentController : public ash::NetworkStateHandlerObserver {
 public:
  using ProgressCallbackList =
      base::RepeatingCallbackList<void(AutoEnrollmentState)>;
  using RlweClientFactory =
      policy::psm::RlweDmserverClientImpl::RlweClientFactory;

  // State of the system clock.
  enum class SystemClockSyncState {
    // This `AutoEnrollmentController` has not tried to wait for the system
    // clock sync state yet.
    kCanWaitForSync,
    // Currently waiting for the system clock to become synchronized.
    kWaitingForSync,
    // Waiting for the system clock to become synchronized timed out.
    kSyncFailed,
    // The system clock is synchronized
    kSynchronized
  };

  explicit AutoEnrollmentController(
      scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory);

  AutoEnrollmentController(const AutoEnrollmentController&) = delete;
  AutoEnrollmentController& operator=(const AutoEnrollmentController&) = delete;

  ~AutoEnrollmentController() override;

  // Starts the auto-enrollment check.  Safe to call multiple times: aborts in
  // case a check is currently running or a decision has already been made.
  void Start();

  // Retry checking.
  void Retry();

  // Returns true if auto-enrollment check is running.
  bool IsInProgress() const;

  // Registers a callback to invoke on state changes.
  base::CallbackListSubscription RegisterProgressCallback(
      const ProgressCallbackList::CallbackType& callback);

  // ash::NetworkStateHandlerObserver:
  void PortalStateChanged(
      const ash::NetworkState* default_network,
      const ash::NetworkState::PortalState portal_state) override;
  void OnShuttingDown() override;

  const std::optional<AutoEnrollmentState>& state() const { return state_; }

  // Returns the auto-enrollment check type performed by this client.
  // The returned value will be `CheckType::kNone` before calling `Start()`.
  AutoEnrollmentTypeChecker::CheckType auto_enrollment_check_type() const {
    return auto_enrollment_check_type_;
  }

  // Sets the factory function that will be used to create the
  // `psm::RlweClient` for tests.
  void SetRlweClientFactoryForTesting(RlweClientFactory test_factory);

  // Sets the factory that will be used to create the `AutoEnrollmentClient`.
  // Ownership is not transferred when calling this - the caller must ensure
  // that the `Factory` pointed to by `auto_enrollment_client_factory` remains
  // valid while this `AutoEnrollmentController` is using it.
  // To use the default factory again, call with nullptr.
  void SetAutoEnrollmentClientFactoryForTesting(
      std::unique_ptr<AutoEnrollmentClient::Factory>
          auto_enrollment_client_factory);

  // Sets factory that will be used to create `EnrollmentStateFetcher`.  To use
  // the default factory again, call with `base::NullCallback()`.
  void SetEnrollmentStateFetcherFactoryForTesting(
      EnrollmentStateFetcher::Factory enrollment_state_fetcher_factory);

  // Returns safeguard timer. Used for testing
  base::OneShotTimer& SafeguardTimerForTesting() { return safeguard_timer_; }

 protected:
  // Complete constructor which can be used to inject testing modules.
  AutoEnrollmentController(
      ash::DeviceSettingsService* device_settings_service,
      DeviceManagementService* device_management_service,
      ServerBackedStateKeysBroker* state_keys_broker,
      ash::NetworkStateHandler* network_state_handler,
      std::unique_ptr<AutoEnrollmentClient::Factory>
          auto_enrollment_client_factory,
      RlweClientFactory psm_rlwe_client_factory,
      EnrollmentStateFetcher::Factory enrollment_state_fetcher_factory,
      scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory);

 private:
  void OnDevDisableBootDetermined(bool dev_disable_boot);

  // Determines the FRE and Initial Enrollment requirement and starts initial
  // enrollment if necessary. If Initial Enrollment would be skipped and the
  // system clock has not been synchronized yet, triggers waiting for system
  // clock sync and will be called again when the system clock state is known.
  void StartWithSystemClockSyncState();

  // Callback for the ownership status check.
  void OnOwnershipStatusCheckDone(
      ash::DeviceSettingsService::OwnershipStatus status);

  // Starts the auto-enrollment client for forced re-enrollment.
  void StartClientForFRE(const std::vector<std::string>& state_keys);

  // Called when the system clock has been synchronized or a timeout has been
  // reached while waiting for the system clock sync.
  void OnSystemClockSyncResult(bool system_clock_synchronized);

  // Starts the auto-enrollment client for initial enrollment.
  void StartClientForInitialEnrollment();

  // Sets `state_` and notifies `progress_callbacks_`.
  void UpdateState(AutoEnrollmentState state);

  // Clears everything that needs to be cleared at OOBE if
  // the device gets the response that forced re-enrollment is not required.
  // This currently removes firmware management parameters and sets
  // block_devmode=0 and check_enrollment=0 in RW_VPD by making asynchronous
  // calls to the respective D-Bus services.
  // The notifications have to be sent only after the FWMP and VPD is cleared,
  // because the user might try to switch to devmode. In this case, if
  // block_devmode is in FWMP and the clear operation didn't finish, the switch
  // would be denied. Also the safeguard timer has to be active until the FWMP
  // is cleared to avoid the risk of blocked flow.
  void StartCleanupForcedReEnrollment();

  // Makes a D-Bus call to cryptohome to remove the firmware management
  // parameters (FWMP) from TPM. Stops the `safeguard_timer_` and notifies the
  // `progress_callbacks_` in case cryptohome does not become available and the
  // timer is still running.
  // `service_is_ready` indicates if the cryptohome D-Bus service is ready.
  void StartRemoveFirmwareManagementParameters(bool service_is_ready);

  // Callback for RemoveFirmwareManagementParameters(). If an error is received
  // here, it is logged only, without changing the flow after that, because
  // the FWMP is used only for newer devices.
  // This also starts the VPD clearing process.
  void OnFirmwareManagementParametersRemoved(
      std::optional<device_management::RemoveFirmwareManagementParametersReply>
          reply);

  // Makes a D-Bus call to session_manager to set block_devmode=0 and
  // check_enrollment=0 in RW_VPD. Stops the `safeguard_timer_` and notifies the
  // `progress_callbacks_` in case session manager does not become available
  // and the timer is still running.
  // `service_is_ready` indicates if the session manager D-Bus service is ready.
  void StartClearForcedReEnrollmentVpd(bool service_is_ready);

  // Callback for ClearForcedReEnrollmentVpd(). If an error is received
  // here, it is logged only, without changing the flow after that.
  // This also notifies the `progress_callbacks_` since the forced re-enrollment
  // cleanup is finished at this point.
  void OnForcedReEnrollmentVpdCleared(bool reply);

  // Handles timeout of the safeguard timer and stops waiting for a result.
  void Timeout();

  // Used for checking ownership.
  raw_ptr<ash::DeviceSettingsService> device_settings_service_;

  // Used for communication with management service.
  raw_ptr<DeviceManagementService> device_management_service_;

  // Used for retrieving device state keys.
  raw_ptr<ServerBackedStateKeysBroker> state_keys_broker_;

  // Used for checking dev boot status.
  std::unique_ptr<EnrollmentFwmpHelper> enrollment_fwmp_helper_;

  std::optional<AutoEnrollmentState> state_;
  ProgressCallbackList progress_callbacks_;

  std::unique_ptr<AutoEnrollmentClient> client_;

  // This will be used to create the `client_`. It can be set using
  // `SetAutoEnrollmentClientFactoryForTesting`.
  std::unique_ptr<AutoEnrollmentClient::Factory>
      auto_enrollment_client_factory_;

  // Constructs the PSM RLWE client. It will either create a fake or real
  // implementation of the client.
  // It is only used for PSM during creating the client for initial enrollment.
  RlweClientFactory psm_rlwe_client_factory_;

  // This timer acts as a belt-and-suspenders safety for the case where one of
  // the asynchronous steps required to make the auto-enrollment decision
  // doesn't come back. Even though in theory they should all terminate, better
  // safe than sorry: There are DBus interactions, an entire network stack etc.
  // - just too many moving pieces to be confident there are no bugs. If
  // something goes wrong, the timer will ensure that a decision gets made
  // eventually, which is crucial to not block OOBE forever. See
  // http://crbug.com/433634 for background.
  // The timer is expected to run during the state determination. The controller
  // is considered idle and can be restarted when the timer is not running.
  base::OneShotTimer safeguard_timer_;

  // Enrollment state fetcher. Invokes `UpdateState` on success or failure.
  std::unique_ptr<EnrollmentStateFetcher> enrollment_state_fetcher_;

  // Factory to create the `enrollment_state_fetcher_`. By default, it is set to
  // `EnrollmentStateFetcher::Create`, but can be overridden with
  // `SetEnrollmentStateFetcherFactoryForTesting`.
  EnrollmentStateFetcher::Factory enrollment_state_fetcher_factory_;

  bool dev_disable_boot_ = false;

  // Which type of auto-enrollment check is being performed by this
  // `AutoEnrollmentClient`.
  AutoEnrollmentTypeChecker::CheckType auto_enrollment_check_type_ =
      AutoEnrollmentTypeChecker::CheckType::kNone;
  bool auto_enrollment_check_type_init_started_ = false;

  // Shared factory for outgoing network requests.
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;

  // Utility for waiting until the system clock has been synchronized.
  std::unique_ptr<ash::SystemClockSyncObservation>
      system_clock_sync_observation_;

  raw_ptr<ash::NetworkStateHandler> network_state_handler_;
  // Observes network state and calls `PortalStateChanged` when it changes from
  // the start until the auto-enrollment state is resolved. Triggers a retry
  // when the device goes online.
  ash::NetworkStateHandlerScopedObservation network_state_observation_{this};

  // Current system clock sync state. This is only modified in
  // `OnSystemClockSyncResult` after `system_clock_sync_wait_requested_` has
  // been set to true.
  SystemClockSyncState system_clock_sync_state_ =
      SystemClockSyncState::kCanWaitForSync;

  // Keeps track of number of tries to request state keys.
  int request_state_keys_tries_ = 0;

  // TODO(igorcov): Merge the two weak_ptr factories in one.
  base::WeakPtrFactory<AutoEnrollmentController> client_start_weak_factory_{
      this};
  base::WeakPtrFactory<AutoEnrollmentController> weak_ptr_factory_{this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_ENROLLMENT_AUTO_ENROLLMENT_CONTROLLER_H_
