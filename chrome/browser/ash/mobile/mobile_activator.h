// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_MOBILE_MOBILE_ACTIVATOR_H_
#define CHROME_BROWSER_ASH_MOBILE_MOBILE_ACTIVATOR_H_

#include <map>
#include <memory>
#include <string>

#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "chromeos/ash/components/network/network_state_handler_observer.h"

namespace base {
class Value;
}  // namespace base

namespace ash {

class NetworkState;
class NetworkStateHandler;
class TestMobileActivator;

// This class performs mobile plan activation process.
//
// There are two types of activation flow:
//
//   1. Over-the-air Service Provision (OTASP) activation
//      a. Call shill Activate() to partially activate modem so it can
//         connect to the network.
//      b. Enable auto-connect on the modem so it will connect to the network
//         in the next step.
//      c. Call shill Activate() again which resets the modem, when the modem
//         comes back, it will auto-connect to the network.
//      d. Navigate to the payment portal.
//      e. Activate the modem using OTASP via shill Activate().
//
//   2. Simple activation - used by non-cellular activation and over-the-air
//      (OTA) activation.
//      a. Ensure there's a network connection.
//      a. Navigate to payment portal.
//      b. Activate the modem via shill CompletetActivation().
class MobileActivator : public NetworkStateHandlerObserver {
 public:
  // Activation state.
  enum class PlanActivationState {
    // Activation WebUI page is loading, activation not started.
    kPageLoading = -1,
    // Activation process started.
    kStart = 0,
    // Initial over the air activation attempt.
    kTryingOTASP = 1,
    // Performing pre-activation process.
    kInitiatingActivation = 3,
    // Reconnecting to network. Used for networks activated over cellular
    // connection.
    kReconnecting = 4,
    // Passively waiting for a network connection. Used for networks activated
    // over non-cellular network.
    kWaitingForConnection = 5,
    // Loading payment portal page.
    kPaymentPortalLoading = 6,
    // Showing payment portal page.
    kShowingPayment = 7,
    // Decides whether to load the portal again or call us done.
    kReconnectingPayment = 8,
    // Delaying activation until payment portal catches up.
    kDelayOTASP = 9,
    // Starting post-payment activation attempt.
    kStartOTASP = 10,
    // Attempting activation.
    kOTASP = 11,
    // Finished activation.
    kDone = 12,
    // Error occurred during activation process.
    kError = 0xFF,
  };

  // Set of errors encountered during activation.
  enum class ActivationError {
    // No error.
    kNone,

    // Activation encountered an error.
    kActivationFailed,

    // Mobile data was disabled.
    kCellularDisabled,

    // Cellular device not present.
    kNoCellularDevice,

    // Cellular service not present.
    kNoCellularService,
  };

  // Activation process observer.
  class Observer {
   public:
    // Signals activation |state| change for given |network|.
    virtual void OnActivationStateChanged(const NetworkState* network,
                                          PlanActivationState state,
                                          ActivationError error) = 0;

   protected:
    Observer() = default;
    virtual ~Observer() = default;
  };

  MobileActivator(const MobileActivator&) = delete;
  MobileActivator& operator=(const MobileActivator&) = delete;

  static MobileActivator* GetInstance();

  // Add/remove activation process observer.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Activation is in process.
  bool RunningActivation() const;
  // Activation state.
  PlanActivationState state() const { return state_; }
  // Initiates activation process.  Can only be called from the UI thread.
  void InitiateActivation(const std::string& service_path);
  // Terminates activation process if already started.
  void TerminateActivation();
  // Process portal load attempt status.
  void OnPortalLoaded(bool success);
  // Process payment transaction status.
  void OnSetTransactionStatus(bool success);

 protected:
  // For unit tests.
  void set_state_for_test(PlanActivationState state) { state_ = state; }
  virtual const NetworkState* GetNetworkState(const std::string& service_path);
  virtual const NetworkState* GetDefaultNetwork();

 private:
  friend struct base::DefaultSingletonTraits<MobileActivator>;
  friend class TestMobileActivator;
  friend class MobileActivatorTest;

  MobileActivator();
  ~MobileActivator() override;

  // NetworkStateHandlerObserver overrides.
  void DefaultNetworkChanged(const NetworkState* network) override;
  void NetworkPropertiesUpdated(const NetworkState* network) override;
  void OnShuttingDown() override;

  void GetPropertiesFailure(const std::string& error_name,
                            base::Value error_data);
  // Handles the signal that the payment portal has finished loading.
  void HandlePortalLoaded(bool success);
  // Handles the signal that the user has finished with the portal.
  void HandleSetTransactionStatus(bool success);
  // Starts activation.
  void StartActivation();
  // Starts activation over non-cellular network.
  void StartActivationOverNonCellularNetwork();
  // Starts OTA activation.
  void StartActivationOTA();
  // Starts OTASP activation.
  void StartActivationOTASP();
  // Called after we delay our OTASP (after payment).
  void RetryOTASP();
  // Continues activation process. This method is called after we disconnect
  // due to detected connectivity issue to kick off reconnection.
  void ContinueConnecting();

  // Starts OTASP process.
  void StartOTASP();
  // Called when an OTASP attempt times out.
  void HandleOTASPTimeout();
  // Connect to network.
  virtual void ConnectNetwork(const NetworkState* network);
  // Forces disconnect / reconnect when we detect portal connectivity issues.
  void ForceReconnect(const NetworkState* network,
                      PlanActivationState next_state);
  // Called when ForceReconnect takes too long to reconnect.
  void ReconnectTimedOut();

  // Called on default network changes to update cellular network activation
  // state.
  void RefreshCellularNetworks();

  // Verify the state of cellular network and modify internal state.
  virtual void EvaluateCellularNetwork(const NetworkState* network);
  // PickNextState selects the desired state based on the current state of the
  // modem and the activator.  It does not transition to this state however.
  PlanActivationState PickNextState(const NetworkState* network) const;
  // One of PickNext*State are called in PickNextState based on whether the
  // modem is online or not.
  PlanActivationState PickNextOnlineState(const NetworkState* network) const;
  PlanActivationState PickNextOfflineState(const NetworkState* network) const;

  // Changes internal state.
  virtual void ChangeState(const NetworkState* network,
                           PlanActivationState new_state,
                           ActivationError error);
  // Resets network devices after cellular activation process.
  void CompleteActivation();

  // Starts the OTASP timeout timer.  If the timer fires, we'll force a
  // disconnect/reconnect cycle on this network.
  virtual void StartOTASPTimer();

  // Records information that cellular plan payment has happened.
  virtual void SignalCellularPlanPayment();

  // Returns true if cellular plan payment has been recorded recently.
  virtual bool HasRecentCellularPlanPayment() const;

  static const char* GetStateDescription(PlanActivationState state);

  // Internal handler state.
  PlanActivationState state_;
  // MEID of cellular device to activate.
  std::string meid_;
  // ICCID of the SIM card on cellular device to activate.
  std::string iccid_;
  // Service path of network being activated. Note that the path can change
  // during the activation process while still representing the same service.
  std::string service_path_;
  // Device on which the network service is activated. While the service path
  // can change during activation due to modem resets, the device path stays
  // the same.
  std::string device_path_;
  // True if activation process has been terminated.
  bool terminated_;
  // Connection retry counter.
  int connection_retry_count_;
  // Counters for how many times we've tried each OTASP step.
  int initial_OTASP_attempts_;
  int trying_OTASP_attempts_;
  int final_OTASP_attempts_;
  // Payment portal reload/reconnect attempt count.
  int payment_reconnect_count_;
  // Timer that monitors how long we spend in error-prone states.
  base::RepeatingTimer state_duration_timer_;

  // State we will return to if we are disconnected.
  PlanActivationState post_reconnect_state_;
  // Called to continue the reconnect attempt.
  base::RepeatingTimer continue_reconnect_timer_;
  // Called when the reconnect attempt times out.
  base::OneShotTimer reconnect_timeout_timer_;
  // Cellular plan payment time.
  base::Time cellular_plan_payment_time_;

  base::ScopedObservation<NetworkStateHandler, NetworkStateHandlerObserver>
      network_state_handler_observer_{this};

  base::ObserverList<Observer>::Unchecked observers_;
  base::WeakPtrFactory<MobileActivator> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_MOBILE_MOBILE_ACTIVATOR_H_
