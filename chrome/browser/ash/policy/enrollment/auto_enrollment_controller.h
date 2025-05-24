// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_ENROLLMENT_AUTO_ENROLLMENT_CONTROLLER_H_
#define CHROME_BROWSER_ASH_POLICY_ENROLLMENT_AUTO_ENROLLMENT_CONTROLLER_H_

#include <memory>
#include <optional>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/policy/enrollment/auto_enrollment_state.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_state_fetcher.h"
#include "chrome/browser/ash/policy/enrollment/psm/rlwe_dmserver_client_impl.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chromeos/ash/components/dbus/device_management/device_management_interface.pb.h"
#include "chromeos/ash/components/network/network_state_handler_observer.h"

namespace ash {
class NetworkStateHandler;
}  // namespace ash

namespace policy {

class DeviceManagementService;
class ServerBackedStateKeysBroker;

// Drives the enrollment state determinatio (for historical reasons called
// auto-enrollment check), running an `EnrollmentStateFetcher` if appropriate to
// make a decision.
// The controller tracks network status to retry when the device is going
// online in case of a prior failure.
class AutoEnrollmentController : public ash::NetworkStateHandlerObserver {
 public:
  using ProgressCallbackList =
      base::RepeatingCallbackList<void(AutoEnrollmentState)>;
  using RlweClientFactory =
      policy::psm::RlweDmserverClientImpl::RlweClientFactory;

  explicit AutoEnrollmentController(
      scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory);

  AutoEnrollmentController(const AutoEnrollmentController&) = delete;
  AutoEnrollmentController& operator=(const AutoEnrollmentController&) = delete;

  ~AutoEnrollmentController() override;

  // Starts the auto-enrollment check.  Safe to call multiple times: aborts in
  // case a check is currently running or a decision has already been made.
  void Start();

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

  // Sets the factory function that will be used to create the
  // `psm::RlweClient` for tests.
  void SetRlweClientFactoryForTesting(RlweClientFactory test_factory);

  // Sets factory that will be used to create `EnrollmentStateFetcher`.  To use
  // the default factory again, call with `base::NullCallback()`.
  void SetEnrollmentStateFetcherFactoryForTesting(
      EnrollmentStateFetcher::Factory enrollment_state_fetcher_factory);

  // Returns safeguard timer. Used for testing
  base::OneShotTimer& SafeguardTimerForTesting() { return safeguard_timer_; }

  // The OOBE network error screen can provide a link to sign in as guest.
  // This should only be allowed if
  //   * enrollment state determination has completed,
  //   * forced enrollment is not strictly required (using guest mode would be
  //     considered an enrollment escape).
  // Use `IsGuestSigninAllowed` to determine if guest mode should be allowed.
  bool IsGuestSigninAllowed() const;

 protected:
  // Complete constructor which can be used to inject testing modules.
  AutoEnrollmentController(
      ash::DeviceSettingsService* device_settings_service,
      DeviceManagementService* device_management_service,
      ServerBackedStateKeysBroker* state_keys_broker,
      ash::NetworkStateHandler* network_state_handler,
      RlweClientFactory psm_rlwe_client_factory,
      EnrollmentStateFetcher::Factory enrollment_state_fetcher_factory,
      scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory);

 private:
  // Sets `state_` and notifies `progress_callbacks_`.
  void UpdateState(AutoEnrollmentState state);

  // Clears everything that needs to be cleared at OOBE if
  // the device gets the response that forced re-enrollment is not required.
  // This currently removes firmware management parameters and sets
  // block_devmode=0 in RW_VPD by making asynchronous calls to the respective
  // D-Bus services.
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

  // Makes a D-Bus call to session_manager to set block_devmode=0 in RW_VPD.
  // Stops the `safeguard_timer_` and notifies the `progress_callbacks_` in case
  // session manager does not become available and the timer is still running.
  // `service_is_ready` indicates if the session manager D-Bus service is ready.
  void StartClearBlockDevmodeVpd(bool service_is_ready);

  // Callback for `StartClearBlockDevmodeVpd`. If clearing block_devmode did
  // not succeed, it is logged only, without changing the flow after that.
  // This also notifies the `progress_callbacks_` since the forced re-enrollment
  // cleanup is finished at this point.
  void OnBlockDevmodeClearedVpd(bool succeeded);

  // Handles timeout of the safeguard timer and stops waiting for a result.
  void Timeout();

  // Used for checking ownership.
  raw_ptr<ash::DeviceSettingsService> device_settings_service_;

  // Used for communication with management service.
  raw_ptr<DeviceManagementService> device_management_service_;

  // Used for retrieving device state keys.
  raw_ptr<ServerBackedStateKeysBroker> state_keys_broker_;

  std::optional<AutoEnrollmentState> state_;
  ProgressCallbackList progress_callbacks_;

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

  // Shared factory for outgoing network requests.
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;

  raw_ptr<ash::NetworkStateHandler> network_state_handler_;
  // Observes network state and calls `PortalStateChanged` when it changes from
  // the start until the auto-enrollment state is resolved. Triggers a retry
  // when the device goes online.
  ash::NetworkStateHandlerScopedObservation network_state_observation_{this};

  base::WeakPtrFactory<AutoEnrollmentController> weak_ptr_factory_{this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_ENROLLMENT_AUTO_ENROLLMENT_CONTROLLER_H_
