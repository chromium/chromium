// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_DEVICE_ACTIVITY_DEVICE_ACTIVITY_CLIENT_H_
#define ASH_COMPONENTS_DEVICE_ACTIVITY_DEVICE_ACTIVITY_CLIENT_H_

#include <memory>

#include "base/component_export.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_state_handler_observer.h"
#include "services/network/public/cpp/resource_request.h"
#include "third_party/private_membership/src/private_membership_rlwe_client.h"
#include "url/gurl.h"

namespace network {
class SimpleURLLoader;
class SharedURLLoaderFactory;
}  // namespace network

class PrefService;

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
  // Tracks the state the client is in, given the use case (i.e DAILY).
  enum class State {
    kIdle,  // Wait on network connection OR |report_timer_| to trigger.
    kCheckingMembershipOprf,   // Phase 1 of the |CheckMembership| request.
    kCheckingMembershipQuery,  // Phase 2 of the |CheckMembership| request.
    kCheckingIn,               // |CheckIn| PSM device active request.
    kHealthCheck,              // Query to perform server health check.
  };

  // Fires device active pings while the device network is connected.
  DeviceActivityClient(
      NetworkStateHandler* handler,
      PrefService* local_state,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const std::string& psm_device_active_secret);
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

 protected:
  // Send Health Check network request and update |state_|.
  // Before calling this method: |state_| is expected to be |kIdle|.
  // After calling this method: |state_| set to |kHealthCheck|.
  virtual void TransitionToHealthCheck();

  // Send Oprf network request and update |state_|.
  // Before calling this method: |state_| is expected to be |kIdle|.
  // After calling this method:  |state_| set to |kCheckingMembershipOprf|.
  virtual void TransitionToCheckMembershipOprf();

  // Update |state_| to |kIdle|.
  virtual void TransitionToIdle();

  // Send Query network request and update |state_|.
  // Before calling this method: |state_| is expected to be
  // |kCheckingMembershipOprf|. After calling this method:  |state_| set to
  // |kCheckingMembershipQuery|.
  virtual void TransitionToCheckMembershipQuery(
      const private_membership::rlwe::PrivateMembershipRlweOprfResponse&
          oprf_response);

  // Send Import network request and update |state_|.
  // Before calling this method: |state_| is expected to be
  // |kCheckingMembershipQuery|. After calling this method:  |state_| set to
  // |kCheckingIn|.
  virtual void TransitionToCheckIn();

 private:
  // Handles device network connecting successfully.
  void OnNetworkOnline();

  // Return Fresnel server network request endpoints determined by the |state_|.
  GURL GetFresnelURL() const;

  // Called when device network comes online as well as by |report_timer_|.
  void TransitionOutOfIdle();

  // Callback from asynchronous method |TransitionToHealthCheck|.
  void OnHealthCheckDone(std::unique_ptr<std::string> response_body);

  // Callback from asynchronous method |TransitionToCheckMembershipOprf|.
  void OnCheckMembershipOprfDone(std::unique_ptr<std::string> response_body);

  // Callback from asynchronous method |TransitionToCheckMembershipQuery|.
  // Check in PSM id based on |response_body| from CheckMembershipQuery.
  void OnCheckMembershipQueryDone(std::unique_ptr<std::string> response_body);

  // Callback from asynchronous method |TransitionToCheckIn|.
  void OnCheckInDone(std::unique_ptr<std::string> response_body);

  // Tracks the current state of the DeviceActivityClient.
  State state_ = State::kIdle;

  // Keep track of whether the device is connected to the network.
  bool network_connected_ = false;

  // API key used to authenticate with the Fresnel server. This key is read from
  // the chrome-internal repository and is not publicly exposed in Chromium.
  const std::string api_key_;

  // Generated on demand each time the state machine leaves the idle state.
  // It is reused by several states. It is reset to nullopt.
  // This field is used apart of PSM Import request.
  absl::optional<std::string> current_day_window_id_;

  // Generated on demand each time the state machine leaves the idle state.
  // It is reused by several states. It is reset to nullopt.
  // This field is used apart of PSM Oprf, Query, and Import requests.
  absl::optional<private_membership::rlwe::RlwePlaintextId> current_day_psm_id_;

  // Time the device last transitioned out of idle state.
  base::Time last_transition_out_of_idle_time_;

  // Tries reporting device actives every |kTimeToRepeat| from when this class
  // is initialized. Time of class initialization depends on when the device is
  // turned on (when |ChromeBrowserMainPartsAsh::PostBrowserStart| is run).
  std::unique_ptr<base::RepeatingTimer> report_timer_;

  // Generated on demand each time the state machine leaves the idle state.
  // Client Generates protos used in request body of Oprf and Query requests.
  std::unique_ptr<private_membership::rlwe::PrivateMembershipRlweClient>
      psm_rlwe_client_;

  // Tracks the visible networks and their properties.
  // |network_state_handler_| outlives the lifetime of this class.
  // |ChromeBrowserMainPartsAsh| initializes the network_state object as
  // part of the |dbus_services_|, before |DeviceActivityClient| is initialized.
  // Similarly, |DeviceActivityClient| is destructed before |dbus_services_|.
  NetworkStateHandler* const network_state_handler_;

  // Update last stored device active ping timestamps for PSM use cases.
  // On powerwash/recovery update |local_state_| to the most recent timestamp
  // |CheckMembership| was performed, as |local_state_| gets deleted.
  // |local_state_| outlives the lifetime of this class.
  // Used local state prefs are initialized by |DeviceActivityController|.
  PrefService* const local_state_;

  // Shared |url_loader_| object used to handle ongoing network requests.
  std::unique_ptr<network::SimpleURLLoader> url_loader_;

  // The URLLoaderFactory we use to issue network requests.
  // |url_loader_factory_| outlives |url_loader_|.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // The ChromeOS platform code will provide a derived PSM device active secret
  // via callback.
  //
  // This secret is used to generate a PSM identifier for the reporting window.
  const std::string psm_device_active_secret_;

  // Automatically cancels callbacks when the referent of weakptr gets
  // destroyed.
  base::WeakPtrFactory<DeviceActivityClient> weak_factory_{this};
};

}  // namespace device_activity
}  // namespace ash

#endif  // ASH_COMPONENTS_DEVICE_ACTIVITY_DEVICE_ACTIVITY_CLIENT_H_
