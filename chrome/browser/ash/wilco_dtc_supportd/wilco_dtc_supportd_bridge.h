// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_WILCO_DTC_SUPPORTD_WILCO_DTC_SUPPORTD_BRIDGE_H_
#define CHROME_BROWSER_ASH_WILCO_DTC_SUPPORTD_WILCO_DTC_SUPPORTD_BRIDGE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ash/wilco_dtc_supportd/wilco_dtc_supportd_notification_controller.h"
#include "chrome/browser/ash/wilco_dtc_supportd/wilco_dtc_supportd_web_request_service.h"
#include "chrome/services/wilco_dtc_supportd/public/mojom/wilco_dtc_supportd.mojom-forward.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/buffer.h"

namespace chromeos {

class WilcoDtcSupportdNetworkContext;

// Establishes Mojo communication to the wilco_dtc_supportd daemon. The Mojo
// pipe gets bootstrapped via D-Bus, and the class takes care of waiting until
// the wilco_dtc_supportd D-Bus service gets started and of repeating the
// bootstrapping after the daemon gets restarted.
class WilcoDtcSupportdBridge final
    : public wilco_dtc_supportd::mojom::WilcoDtcSupportdClient {
 public:
  // Delegate class, allowing to stub out unwanted operations in unit tests.
  class Delegate {
   public:
    virtual ~Delegate();

    // Creates a Mojo invitation that requests the remote implementation of the
    // WilcoDtcSupportdServiceFactory interface.
    // Returns |wilco_dtc_supportd_service_factory_mojo_remote| - remote
    // that points to the remote implementation of the interface,
    // |remote_endpoint_fd| - file descriptor of the remote endpoint to be sent.
    virtual void CreateWilcoDtcSupportdServiceFactoryMojoInvitation(
        mojo::Remote<wilco_dtc_supportd::mojom::WilcoDtcSupportdServiceFactory>*
            wilco_dtc_supportd_service_factory_mojo_remote,
        base::ScopedFD* remote_endpoint_fd) = 0;
  };

  // Returns the global singleton instance.
  static WilcoDtcSupportdBridge* Get();

  static base::TimeDelta connection_attempt_interval_for_testing();
  static int max_connection_attempt_count_for_testing();

  explicit WilcoDtcSupportdBridge(
      std::unique_ptr<WilcoDtcSupportdNetworkContext> network_context);
  // For use in tests.
  WilcoDtcSupportdBridge(
      std::unique_ptr<Delegate> delegate,
      std::unique_ptr<WilcoDtcSupportdNetworkContext> network_context,
      std::unique_ptr<WilcoDtcSupportdNotificationController>
          notification_controller);

  ~WilcoDtcSupportdBridge() override;

  // Sets the Wilco DTC configuration data, passed and owned by the
  // |WilcoDtcSupportdManager| from the device policy.
  // The nullptr should be passed to clear it.
  void SetConfigurationData(const std::string* data);
  const std::string& GetConfigurationDataForTesting();

  // Mojo proxy to the WilcoDtcSupportdService implementation in the
  // wilco_dtc_supportd daemon. Returns null when bootstrapping of Mojo
  // connection hasn't started yet. Note that, however, non-null is already
  // returned before the bootstrapping fully completes.
  wilco_dtc_supportd::mojom::WilcoDtcSupportdServiceProxy*
  wilco_dtc_supportd_service_mojo_proxy() {
    return wilco_dtc_supportd_service_mojo_remote_
               ? wilco_dtc_supportd_service_mojo_remote_.get()
               : nullptr;
  }

 private:
  // Starts waiting until the wilco_dtc_supportd D-Bus service becomes available
  // (or until this waiting fails).
  void WaitForDBusService();
  // Schedules a postponed execution of WaitForDBusService().
  void ScheduleWaitingForDBusService();
  // Called once waiting for the D-Bus service, started by WaitForDBusService(),
  // finishes.
  void OnWaitedForDBusService(bool service_is_available);
  // Triggers Mojo bootstrapping via a D-Bus to the wilco_dtc_supportd daemon.
  void BootstrapMojoConnection();
  // Called once the result of the D-Bus call, made from
  // BootstrapMojoConnection(), arrives.
  void OnBootstrappedMojoConnection(bool success);
  // Called once the GetService() Mojo request completes.
  void OnMojoGetServiceCompleted();
  // Called when Mojo signals a connection error.
  void OnMojoConnectionError();

  // wilco_dtc_supportd::mojom::WilcoDtcSupportdClient overrides.
  void PerformWebRequest(
      wilco_dtc_supportd::mojom::WilcoDtcSupportdWebRequestHttpMethod
          http_method,
      mojo::ScopedHandle url,
      std::vector<mojo::ScopedHandle> headers,
      mojo::ScopedHandle request_body,
      PerformWebRequestCallback callback) override;
  void SendWilcoDtcMessageToUi(
      mojo::ScopedHandle json_message,
      SendWilcoDtcMessageToUiCallback callback) override;
  void GetConfigurationData(GetConfigurationDataCallback callback) override;
  void HandleEvent(
      wilco_dtc_supportd::mojom::WilcoDtcSupportdEvent event) override;
  void GetCrosHealthdDiagnosticsService(
      cros_healthd::mojom::CrosHealthdDiagnosticsServiceRequest service)
      override;
  void GetCrosHealthdProbeService(
      cros_healthd::mojom::CrosHealthdProbeServiceRequest service) override;

  std::unique_ptr<Delegate> delegate_;

  // Mojo receiver that binds |this| as an implementation of the
  // WilcoDtcSupportdClient Mojo interface.
  mojo::Receiver<wilco_dtc_supportd::mojom::WilcoDtcSupportdClient>
      mojo_self_receiver_{this};

  // Current consecutive connection attempt number.
  int connection_attempt_ = 0;

  // Remotes to the Mojo services exposed by the wilco_dtc_supportd daemon.
  mojo::Remote<wilco_dtc_supportd::mojom::WilcoDtcSupportdServiceFactory>
      wilco_dtc_supportd_service_factory_mojo_remote_;
  mojo::Remote<wilco_dtc_supportd::mojom::WilcoDtcSupportdService>
      wilco_dtc_supportd_service_mojo_remote_;

  // The service to perform wilco_dtc_supportd's web requests.
  WilcoDtcSupportdWebRequestService web_request_service_;

  // The wilco_dtc_supportd notification controller in charge of sending
  // appropriate UI notifications.
  std::unique_ptr<WilcoDtcSupportdNotificationController>
      notification_controller_ = nullptr;

  // The Wilco DTC configuration data blob, passed from the device policy, is
  // stored and owned by |WilcoDtcSupportdManager|.
  // nullptr if there is no available configuration data for the Wilco DTC.
  const std::string* configuration_data_ = nullptr;

  // These weak pointer factories must be the last members:

  // Used for cancelling previously posted tasks that wait for the D-Bus service
  // availability.
  base::WeakPtrFactory<WilcoDtcSupportdBridge> dbus_waiting_weak_ptr_factory_{
      this};
  base::WeakPtrFactory<WilcoDtcSupportdBridge> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WilcoDtcSupportdBridge);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_WILCO_DTC_SUPPORTD_WILCO_DTC_SUPPORTD_BRIDGE_H_
