// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_DEVICE_ACTIVITY_DEVICE_ACTIVITY_CLIENT_H_
#define ASH_COMPONENTS_DEVICE_ACTIVITY_DEVICE_ACTIVITY_CLIENT_H_

#include <memory>

#include "base/component_export.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_state_handler_observer.h"

namespace ash {
namespace device_activity {

// Observes the network for connected state to determine whether the device
// is active in a given window.
// State Transition flow:
// kIdle -> kCheckingMembershipOprf -> kCheckingMembershipQuery
// -> kIdle or (kCheckingIn -> kIdle)
class COMPONENT_EXPORT(ASH_DEVICE_ACTIVITY) DeviceActivityClient
    : public chromeos::NetworkStateHandlerObserver {
 public:
  // For a given use case (DAILY), tracks the state the client is currently in.
  enum class State {
    kIdle,  // Wait on network connection OR |report_timer_| to trigger.
    kCheckingMembershipOprf,   // Phase 1 of the |CheckMembership| request.
    kCheckingMembershipQuery,  // Phase 2 of the |CheckMembership| request.
    kCheckingIn,               // |CheckIn| PSM device active request.
    kHealthCheck,              // Query to perform server health check.
  };

  // Constructor fires device active pings while the device network is
  // connected.
  DeviceActivityClient(NetworkStateHandler* handler);
  DeviceActivityClient(const DeviceActivityClient&) = delete;
  DeviceActivityClient& operator=(const DeviceActivityClient&) = delete;
  ~DeviceActivityClient() override;

  // Initialize repeating timer to run every |kTimeToRepeat|.
  virtual std::unique_ptr<base::RepeatingTimer> ConstructReportTimer();

  // Returns pointer to |report_timer_|.
  base::RepeatingTimer* GetReportTimer();

  // NetworkStateHandlerObserver overridden method.
  void DefaultNetworkChanged(const NetworkState* network) override;

  State GetState() const;

 private:
  // Handles device network connecting successfully.
  void OnNetworkOnline();

  // Called when device network comes online as well as by |report_timer_|.
  void TransitionOutOfIdle();

  // Send Health Check network request and update |state_|.
  // Before method: |state_| set to |kIdle|.
  // After method: |state_| set to |kHealthCheck|.
  void TransitionToHealthCheck();

  // Callback from asynchronous method |TransitionToHealthCheck|.
  void OnHealthCheckDone();

  // Send Oprf network request and update |state_|.
  // Before method: |state_| set to |kIdle|.
  // After method:  |state_| set to |kCheckingMembershipOprf|.
  void TransitionToCheckMembershipOprf();

  // Callback from asynchronous method |TransitionToCheckMembershipOprf|.
  void OnCheckMembershipOprfDone();

  // Send Query network request and update |state_|.
  // Before method: |state_| set to |kCheckingMembershipOprf|.
  // After method:  |state_| set to |kCheckingMembershipQuery|.
  void TransitionToCheckMembershipQuery();

  // Callback from asynchronous method |TransitionToCheckMembershipQuery|.
  // |needs_check_in| is true if check membership query response returns false.
  // |needs_check_in| is false if check membership query response returns true.
  void OnCheckMembershipQueryDone(bool needs_check_in);

  // Send Import network request and update |state_|.
  // Before method: |state_| set to |kCheckingMembershipQuery|.
  // After method:  |state_| set to |kCheckingIn|.
  void TransitionToCheckIn();

  // Callback from asynchronous method |TransitionToCheckIn|.
  void OnCheckInDone();

  // Update |state_| to |kIdle|.
  void TransitionToIdle();

  // Tracks the current state of the DeviceActivityClient.
  State state_ = State::kIdle;

  // Keep track of whether the device is connected to the network.
  bool network_connected_ = false;

  // TODO(hirthanan): Retrieve the derived secret from VPD.
  // The ChromeOS platform code will provide a derived stable device secret.
  // This secret is used to generate a PSM identifier for the reporting window.
  const std::string derived_stable_device_secret_;

  // Time the network last connected successfully.
  base::Time last_time_network_came_online_;

  // Tries reporting device actives every |kTimeToRepeat| from when this class
  // is initialized. Time of class initialization depends on when the device is
  // turned on (chrome_browser_main_chromeos.cc |PostBrowserStart|).
  std::unique_ptr<base::RepeatingTimer> report_timer_;

  // Tracks the visible networks and their properties.
  // |network_state_handler_| outlives the lifetime of this class.
  // |ChromeBrowserMainPartsAsh| initializes the network_state object as
  // part of the |dbus_services_|, before |DeviceActivityClient| is initialized.
  // Similarly, |DeviceActivityClient| is destructed before |dbus_services_|.
  NetworkStateHandler* const network_state_handler_;
};

}  // namespace device_activity
}  // namespace ash

#endif  // ASH_COMPONENTS_DEVICE_ACTIVITY_DEVICE_ACTIVITY_CLIENT_H_
