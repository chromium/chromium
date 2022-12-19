// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_TARGET_DEVICE_BOOTSTRAP_CONTROLLER_H_
#define CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_TARGET_DEVICE_BOOTSTRAP_CONTROLLER_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/target_device_connection_broker.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

class NearbyConnectionsManager;

namespace ash::quick_start {

class AuthenticatedConnection;
class IncomingConnection;

class TargetDeviceBootstrapController
    : public TargetDeviceConnectionBroker::ConnectionLifecycleListener {
 public:
  explicit TargetDeviceBootstrapController(
      base::WeakPtr<NearbyConnectionsManager> nearby_connections_manager);
  TargetDeviceBootstrapController(TargetDeviceBootstrapController&) = delete;
  TargetDeviceBootstrapController& operator=(TargetDeviceBootstrapController&) =
      delete;
  ~TargetDeviceBootstrapController() override;

  enum class Step {
    NONE,
    ERROR,
    ADVERTISING,
    QR_CODE_VERIFICATION,
    CONNECTED,
  };

  enum class ErrorCode {
    START_ADVERTISING_FAILED,
    CONNECTION_REJECTED,
    CONNECTION_CLOSED,
  };

  using QRCodePixelData = std::vector<uint8_t>;

  using Payload = absl::variant<absl::monostate, ErrorCode, QRCodePixelData>;

  struct Status {
    Status();
    ~Status();
    Step step = Step::NONE;
    Payload payload;
  };

  class Observer : public base::CheckedObserver {
   public:
    virtual void OnStatusChanged(const Status& status) = 0;

   protected:
    ~Observer() override = default;
  };

  void AddObserver(Observer* obs);
  void RemoveObserver(Observer* obs);

  void GetFeatureSupportStatusAsync(
      TargetDeviceConnectionBroker::FeatureSupportStatusCallback callback);

  // This function would crash (if DCHECKs are on) in case there are existing
  // valid weakptrs.
  base::WeakPtr<TargetDeviceBootstrapController> GetAsWeakPtrForClient();

  // TODO: Finalize api for frontend.
  void StartAdvertising();
  void StopAdvertising();

  // TargetDeviceConnectionBroker::ConnectionLifecycleListener:
  void OnIncomingConnectionInitiated(
      const std::string& source_device_id,
      base::WeakPtr<IncomingConnection> connection) override;
  void OnConnectionAuthenticated(
      const std::string& source_device_id,
      base::WeakPtr<AuthenticatedConnection> connection) override;
  void OnConnectionRejected(const std::string& source_device_id) override;
  void OnConnectionClosed(const std::string& source_device_id) override;

 private:
  void NotifyObservers();
  void OnStartAdvertisingResult(bool success);
  void OnStopAdvertising();
  std::unique_ptr<TargetDeviceConnectionBroker> connection_broker_;

  std::string source_device_id_;
  base::WeakPtr<IncomingConnection> incoming_connection_;
  // TODO: Should we enforce one observer at a time here too?
  base::ObserverList<Observer> observers_;

  Status status_;

  base::WeakPtrFactory<TargetDeviceBootstrapController>
      weak_ptr_factory_for_clients_{this};

  base::WeakPtrFactory<TargetDeviceBootstrapController> weak_ptr_factory_{this};
};

}  // namespace ash::quick_start

#endif  // CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_TARGET_DEVICE_BOOTSTRAP_CONTROLLER_H_
